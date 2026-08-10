#include "searchcontroller.h"

#include "applicationlauncher.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>

namespace {

constexpr int MaximumActionResults = 6;
constexpr int MaximumApplicationResults = 10;
constexpr int MaximumFileResults = 24;
constexpr int MaximumScannedEntries = 20000;

struct SearchAction {
    const char *id;
    const char *title;
    const char *subtitle;
    const char *icon;
    const char *keywords;
};

constexpr SearchAction Actions[] = {
    {"applications", "Applications", "Browse installed applications", "applications", "apps launcher overview"},
    {"files", "Files", "Browse your Northstar home folder", "files", "home folders documents"},
    {"settings", "Settings", "Change Northstar preferences", "settings", "preferences appearance desktop"},
    {"software", "Software Center", "Review installed software", "software", "packages applications updates"},
    {"terminal", "Terminal", "Open a command-line session", "terminal", "console qterminal shell"},
    {"browser", "Firefox", "Browse the web", "browser", "firefox web browser"},
};

QStringList queryTerms(const QString &query)
{
    return query.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QVariantMap resultRecord(const QString &kind,
                         const QString &id,
                         const QString &title,
                         const QString &subtitle,
                         const QString &icon,
                         const QVariant &activationData,
                         const QString &category,
                         const QVariant &iconSource = {},
                         bool isDirectory = false)
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("subtitle"), subtitle},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("iconSource"), iconSource},
        {QStringLiteral("activationData"), activationData},
        {QStringLiteral("category"), category},
        {QStringLiteral("isDirectory"), isDirectory},
    };
}

} // namespace

SearchController::SearchController(ApplicationLauncher *launcher,
                                   QObject *parent,
                                   QString homePath)
    : QObject(parent)
    , m_launcher(launcher)
    , m_homePath(homePath.trimmed().isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            : std::move(homePath))
{
    m_homePath = QDir::cleanPath(QDir::fromNativeSeparators(m_homePath));
    m_debounceTimer.setInterval(140);
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchController::beginFileSearch);
    if (m_launcher != nullptr) {
        connect(m_launcher, &ApplicationLauncher::applicationsChanged,
                this, &SearchController::rebuildImmediateResults);
    }
    rebuildImmediateResults();
}

QString SearchController::query() const
{
    return m_query;
}

QVariantList SearchController::results() const
{
    return m_results;
}

bool SearchController::searching() const
{
    return m_searching;
}

void SearchController::clear()
{
    setQuery({});
}

void SearchController::setQuery(const QString &query)
{
    const QString boundedQuery = query.left(256);
    if (m_query == boundedQuery) {
        return;
    }

    ++m_generation;
    m_debounceTimer.stop();
    if (m_cancelToken) {
        m_cancelToken->store(true);
    }
    m_cancelToken.reset();
    setSearching(false);
    m_query = boundedQuery;
    emit queryChanged();
    rebuildImmediateResults();

    if (m_query.trimmed().size() >= 2 && QDir(m_homePath).exists()) {
        setSearching(true);
        m_debounceTimer.start();
    }
}

bool SearchController::activateResult(int index)
{
    if (index < 0 || index >= m_results.size()) {
        return false;
    }

    const QVariantMap result = m_results.at(index).toMap();
    const QString kind = result.value(QStringLiteral("kind")).toString();
    const QString activationData = result.value(QStringLiteral("activationData")).toString();
    if (kind == QStringLiteral("action")) {
        static const QSet<QString> allowedActions = {
            QStringLiteral("applications"), QStringLiteral("files"),
            QStringLiteral("settings"), QStringLiteral("software"),
            QStringLiteral("terminal"), QStringLiteral("browser"),
        };
        if (!allowedActions.contains(activationData)) {
            return false;
        }
        emit actionRequested(activationData);
        return true;
    }

    if (kind == QStringLiteral("application") && m_launcher != nullptr) {
        const QVariantList applications = m_launcher->applications();
        const bool known = std::any_of(applications.cbegin(), applications.cend(),
                                       [&activationData](const QVariant &entry) {
            return entry.toMap().value(QStringLiteral("desktopId")).toString() == activationData;
        });
        if (!known) {
            return false;
        }
        emit applicationRequested(activationData);
        return true;
    }

    if (kind == QStringLiteral("file") || kind == QStringLiteral("folder")) {
        const QFileInfo info(activationData);
        const QString canonicalPath = info.canonicalFilePath();
        if (canonicalPath.isEmpty() || !pathWithin(canonicalPath, m_homePath)
            || (kind == QStringLiteral("folder")) != info.isDir()) {
            return false;
        }
        emit fileRequested(canonicalPath, info.isDir());
        return true;
    }

    return false;
}

bool SearchController::matchesTerms(const QString &candidate, const QStringList &terms)
{
    return std::all_of(terms.cbegin(), terms.cend(), [&candidate](const QString &term) {
        return candidate.contains(term, Qt::CaseInsensitive);
    });
}

QString SearchController::applicationIcon(const QVariantMap &application)
{
    const QString descriptor = QStringLiteral("%1 %2 %3")
        .arg(application.value(QStringLiteral("desktopId")).toString(),
             application.value(QStringLiteral("name")).toString(),
             application.value(QStringLiteral("categories")).toStringList().join(QLatin1Char(' ')))
        .toLower();
    if (descriptor.contains(QStringLiteral("terminal"))) {
        return QStringLiteral("terminal");
    }
    if (descriptor.contains(QStringLiteral("firefox")) || descriptor.contains(QStringLiteral("browser"))) {
        return QStringLiteral("browser");
    }
    if (descriptor.contains(QStringLiteral("text")) || descriptor.contains(QStringLiteral("editor"))) {
        return QStringLiteral("editor");
    }
    if (descriptor.contains(QStringLiteral("software"))) {
        return QStringLiteral("software");
    }
    if (descriptor.contains(QStringLiteral("welcome"))) {
        return QStringLiteral("welcome");
    }
    return QStringLiteral("applications");
}

void SearchController::rebuildImmediateResults()
{
    const QString normalizedQuery = m_query.trimmed();
    const QStringList terms = queryTerms(normalizedQuery);
    QVariantList immediate;

    int actionCount = 0;
    for (const SearchAction &action : Actions) {
        const QString candidate = QStringLiteral("%1 %2 %3 %4")
            .arg(QString::fromLatin1(action.id), QString::fromLatin1(action.title),
                 QString::fromLatin1(action.subtitle), QString::fromLatin1(action.keywords));
        if (!terms.isEmpty() && !matchesTerms(candidate, terms)) {
            continue;
        }
        immediate.append(resultRecord(
            QStringLiteral("action"), QStringLiteral("action:") + QString::fromLatin1(action.id),
            QString::fromLatin1(action.title), QString::fromLatin1(action.subtitle),
            QString::fromLatin1(action.icon), QString::fromLatin1(action.id),
            QStringLiteral("Actions")));
        if (++actionCount >= MaximumActionResults) {
            break;
        }
    }

    int applicationCount = 0;
    if (m_launcher != nullptr) {
        for (const QVariant &entry : m_launcher->applications()) {
            const QVariantMap application = entry.toMap();
            const QString candidate = QStringLiteral("%1 %2 %3 %4")
                .arg(application.value(QStringLiteral("name")).toString(),
                     application.value(QStringLiteral("categories")).toStringList().join(QLatin1Char(' ')),
                     application.value(QStringLiteral("desktopId")).toString(),
                     application.value(QStringLiteral("sourceType")).toString());
            if (!terms.isEmpty() && !matchesTerms(candidate, terms)) {
                continue;
            }
            const QString desktopId = application.value(QStringLiteral("desktopId")).toString();
            const QString sourceType = application.value(QStringLiteral("sourceType")).toString();
            immediate.append(resultRecord(
                QStringLiteral("application"), QStringLiteral("application:") + desktopId,
                application.value(QStringLiteral("name")).toString(),
                sourceType == QStringLiteral("bundle")
                    ? QStringLiteral("Northstar application") : QStringLiteral("Installed application"),
                applicationIcon(application), desktopId, QStringLiteral("Applications"),
                application.value(QStringLiteral("iconSource"))));
            if (++applicationCount >= MaximumApplicationResults) {
                break;
            }
        }
    }

    m_immediateResults = immediate;
    m_results = immediate;
    emit resultsChanged();
}

void SearchController::beginFileSearch()
{
    const QString normalizedQuery = m_query.trimmed();
    if (normalizedQuery.size() < 2) {
        setSearching(false);
        return;
    }

    const quint64 generation = m_generation;
    auto token = std::make_shared<std::atomic_bool>(false);
    m_cancelToken = token;
    auto *watcher = new QFutureWatcher<FileSearchResult>(this);
    connect(watcher, &QFutureWatcher<FileSearchResult>::finished, this,
            [this, watcher, generation, token]() {
        const FileSearchResult fileResult = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation || token->load()) {
            return;
        }
        m_results = m_immediateResults;
        m_results.append(fileResult.results);
        emit resultsChanged();
        setSearching(false);
    });
    watcher->setFuture(QtConcurrent::run(&SearchController::searchHome,
                                         m_homePath, normalizedQuery, token));
}

SearchController::FileSearchResult SearchController::searchHome(
    const QString &homePath,
    const QString &query,
    const std::shared_ptr<std::atomic_bool> &cancelled)
{
    struct Match {
        QFileInfo info;
        QString relativePath;
        int score = 0;
    };

    FileSearchResult result;
    const QDir home(homePath);
    const QString canonicalHome = QFileInfo(home.absolutePath()).canonicalFilePath();
    if (canonicalHome.isEmpty()) {
        return result;
    }

    const QStringList terms = queryTerms(query);
    QList<Match> matches;
    QStringList pendingDirectories{canonicalHome};
    int scannedEntries = 0;
    while (!pendingDirectories.isEmpty() && scannedEntries < MaximumScannedEntries) {
        if (cancelled->load()) {
            return {};
        }
        const QString directoryPath = pendingDirectories.takeFirst();
        const QFileInfoList entries = QDir(directoryPath).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &info : entries) {
            if (cancelled->load()) {
                return {};
            }
            if (++scannedEntries > MaximumScannedEntries) {
                result.truncated = true;
                break;
            }
            if (info.isSymLink() || info.fileName().startsWith(QLatin1Char('.'))) {
                continue;
            }
            const QString canonicalPath = info.canonicalFilePath();
            if (canonicalPath.isEmpty() || !pathWithin(canonicalPath, canonicalHome)) {
                continue;
            }
            const QString relativePath = QDir(canonicalHome).relativeFilePath(canonicalPath);
            if (info.isDir()) {
                pendingDirectories.append(canonicalPath);
            }
            const QString candidate = info.fileName() + QLatin1Char(' ') + relativePath;
            if (!matchesTerms(candidate, terms)) {
                continue;
            }
            int score = 2;
            if (info.fileName().compare(query, Qt::CaseInsensitive) == 0) {
                score = 0;
            } else if (info.fileName().startsWith(query, Qt::CaseInsensitive)) {
                score = 1;
            }
            matches.append({info, relativePath, score});
        }
    }

    std::sort(matches.begin(), matches.end(), [](const Match &left, const Match &right) {
        if (left.score != right.score) {
            return left.score < right.score;
        }
        if (left.info.isDir() != right.info.isDir()) {
            return left.info.isDir();
        }
        return QString::compare(left.relativePath, right.relativePath, Qt::CaseInsensitive) < 0;
    });

    const int count = std::min(MaximumFileResults, static_cast<int>(matches.size()));
    for (int index = 0; index < count; ++index) {
        const Match &match = matches.at(index);
        result.results.append(resultRecord(
            match.info.isDir() ? QStringLiteral("folder") : QStringLiteral("file"),
            QStringLiteral("path:") + match.info.canonicalFilePath(), match.info.fileName(),
            QStringLiteral("~/") + match.relativePath,
            match.info.isDir() ? QStringLiteral("folder") : QStringLiteral("file"),
            match.info.canonicalFilePath(), QStringLiteral("Files"), {}, match.info.isDir()));
    }
    result.truncated = result.truncated || matches.size() > MaximumFileResults;
    return result;
}

bool SearchController::pathWithin(const QString &path, const QString &root)
{
    const QString normalizedPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    QString normalizedRoot = QFileInfo(root).canonicalFilePath();
    if (normalizedRoot.isEmpty()) {
        normalizedRoot = QDir::cleanPath(QDir::fromNativeSeparators(root));
    }
    return normalizedPath == normalizedRoot
        || normalizedPath.startsWith(normalizedRoot + QLatin1Char('/'));
}

void SearchController::setSearching(bool searching)
{
    if (m_searching == searching) {
        return;
    }
    m_searching = searching;
    emit searchingChanged();
}
