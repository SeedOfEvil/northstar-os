#include "applicationcatalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

QString unescapeDesktopValue(const QString &value)
{
    QString result;
    result.reserve(value.size());

    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character != QLatin1Char('\\') || index + 1 >= value.size()) {
            result.append(character);
            continue;
        }

        const QChar escaped = value.at(++index);
        switch (escaped.unicode()) {
        case 's':
            result.append(QLatin1Char(' '));
            break;
        case 'n':
            result.append(QLatin1Char('\n'));
            break;
        case 't':
            result.append(QLatin1Char('\t'));
            break;
        case 'r':
            result.append(QLatin1Char('\r'));
            break;
        case '\\':
            result.append(QLatin1Char('\\'));
            break;
        default:
            result.append(QLatin1Char('\\'));
            result.append(escaped);
            break;
        }
    }

    return result;
}

QString localizedValue(const QHash<QString, QString> &values, const QString &key)
{
    QStringList candidates;
    const QLocale locale;
    candidates.append(locale.name());
    for (const QString &language : locale.uiLanguages()) {
        candidates.append(language);
        candidates.append(QString(language).replace(QLatin1Char('-'), QLatin1Char('_')));
    }

    for (const QString &candidate : std::as_const(candidates)) {
        const QString localizedKey = key + QLatin1Char('[') + candidate + QLatin1Char(']');
        if (values.contains(localizedKey)) {
            return unescapeDesktopValue(values.value(localizedKey)).trimmed();
        }
    }

    return unescapeDesktopValue(values.value(key)).trimmed();
}

bool desktopBoolean(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.trimmed() == QStringLiteral("1");
}

QStringList desktopList(const QString &value)
{
    QStringList result;
    for (const QString &item : unescapeDesktopValue(value).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        result.append(item.trimmed());
    }
    return result;
}

bool containsNorthstar(const QStringList &values)
{
    return std::any_of(values.cbegin(), values.cend(), [](const QString &value) {
        return value.compare(QStringLiteral("Northstar"), Qt::CaseInsensitive) == 0;
    });
}

bool tryExecutable(const QString &value)
{
    const QString executable = value.trimmed();
    if (executable.isEmpty()) {
        return true;
    }

    if (executable.contains(QLatin1Char('/'))) {
        return QFileInfo(executable).isExecutable();
    }

    return !QStandardPaths::findExecutable(executable).isEmpty();
}

QString desktopIdForPath(const QDir &root, const QString &path)
{
    QString desktopId = root.relativeFilePath(path);
    desktopId.replace(QDir::separator(), QLatin1Char('/'));
    if (desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        desktopId.chop(8);
    }
    return desktopId;
}

} // namespace

ApplicationCatalog::ApplicationCatalog(QStringList applicationDirectories, QObject *parent)
    : QObject(parent)
    , m_applicationDirectories(std::move(applicationDirectories))
    , m_watcher(this)
    , m_refreshTimer(this)
{
    if (m_applicationDirectories.isEmpty()) {
        m_applicationDirectories = defaultApplicationDirectories();
    }

    m_refreshTimer.setInterval(150);
    m_refreshTimer.setSingleShot(true);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &ApplicationCatalog::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ApplicationCatalog::scheduleReload);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        reload();
    });

    reload();
}

QList<DesktopApplication> ApplicationCatalog::entries() const
{
    return m_entries;
}

QVariantList ApplicationCatalog::applications() const
{
    return toVariantList(m_entries);
}

QVariantList ApplicationCatalog::searchApplications(const QString &query) const
{
    const QStringList terms = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        return applications();
    }

    QList<DesktopApplication> matches;
    for (const DesktopApplication &application : m_entries) {
        const QString searchText = QStringList{
            application.name,
            application.genericName,
            application.desktopId,
            application.categories.join(QLatin1Char(' '))
        }.join(QLatin1Char(' '));

        const bool matchesAllTerms = std::all_of(terms.cbegin(), terms.cend(), [&searchText](const QString &term) {
            return searchText.contains(term, Qt::CaseInsensitive);
        });
        if (matchesAllTerms) {
            matches.append(application);
        }
    }

    return toVariantList(matches);
}

QVariantList ApplicationCatalog::toVariantList(const QList<DesktopApplication> &entries)
{
    QVariantList result;
    result.reserve(entries.size());

    for (const DesktopApplication &application : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("desktopId"), application.desktopId);
        item.insert(QStringLiteral("name"), application.name);
        item.insert(QStringLiteral("genericName"), application.genericName);
        item.insert(QStringLiteral("icon"), application.icon);
        item.insert(QStringLiteral("categories"), application.categories);
        item.insert(QStringLiteral("sourceType"), QStringLiteral("desktop"));
        item.insert(QStringLiteral("sourcePath"), application.sourcePath);
        item.insert(QStringLiteral("exec"), application.exec);
        item.insert(QStringLiteral("launchable"), application.launchable);
        item.insert(QStringLiteral("mimeTypes"), application.mimeTypes);

        QVariantList actions;
        for (const DesktopAction &action : application.actions) {
            actions.append(QVariantMap{
                {QStringLiteral("id"), action.id},
                {QStringLiteral("name"), action.name},
                {QStringLiteral("icon"), action.icon},
            });
        }
        item.insert(QStringLiteral("actions"), actions);
        result.append(item);
    }

    return result;
}

QStringList ApplicationCatalog::applicationIds() const
{
    QStringList result;
    result.reserve(m_entries.size());
    for (const DesktopApplication &application : m_entries) {
        result.append(application.desktopId);
    }
    return result;
}

bool ApplicationCatalog::reload()
{
    QList<DesktopApplication> discovered;
    QSet<QString> seenDesktopIds;

    for (const QString &directoryPath : std::as_const(m_applicationDirectories)) {
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        QStringList files;
        QDirIterator iterator(
            directory.absolutePath(),
            QStringList{QStringLiteral("*.desktop")},
            QDir::Files | QDir::Readable,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            files.append(iterator.next());
        }

        std::sort(files.begin(), files.end(), [](const QString &left, const QString &right) {
            return QString::compare(left, right, Qt::CaseInsensitive) < 0;
        });

        for (const QString &filePath : std::as_const(files)) {
            const QString desktopId = desktopIdForPath(directory, filePath);
            if (desktopId.isEmpty() || seenDesktopIds.contains(desktopId)) {
                continue;
            }

            // The first XDG data directory wins. This also lets Hidden and
            // NoDisplay entries mask a lower-priority copy of the same id.
            seenDesktopIds.insert(desktopId);

            DesktopApplication application;
            if (readDesktopEntry(filePath, desktopId, &application)) {
                discovered.append(application);
            }
        }
    }

    std::sort(discovered.begin(), discovered.end(), [](const DesktopApplication &left, const DesktopApplication &right) {
        const int nameComparison = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        if (nameComparison != 0) {
            return nameComparison < 0;
        }
        return QString::compare(left.desktopId, right.desktopId, Qt::CaseInsensitive) < 0;
    });

    refreshWatchPaths();

    if (discovered == m_entries) {
        return false;
    }

    m_entries = std::move(discovered);
    emit applicationsChanged();
    return true;
}

bool ApplicationCatalog::launchSpec(const QString &desktopId, QString *program, QStringList *arguments) const
{
    if (program == nullptr || arguments == nullptr) {
        return false;
    }

    const auto match = std::find_if(m_entries.cbegin(), m_entries.cend(), [&desktopId](const DesktopApplication &application) {
        return application.desktopId == desktopId;
    });
    if (match == m_entries.cend()) {
        return false;
    }

    const QStringList command = expandExec(*match);
    if (command.isEmpty()) {
        return false;
    }

    *program = command.constFirst();
    *arguments = command.mid(1);
    return !program->isEmpty();
}

bool ApplicationCatalog::actionLaunchSpec(const QString &desktopId, const QString &actionId,
                                          QString *program, QStringList *arguments) const
{
    if (program == nullptr || arguments == nullptr) {
        return false;
    }

    const auto match = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                    [&desktopId](const DesktopApplication &application) {
        return application.desktopId == desktopId;
    });
    if (match == m_entries.cend()) {
        return false;
    }

    const auto action = std::find_if(match->actions.cbegin(), match->actions.cend(),
                                     [&actionId](const DesktopAction &candidate) {
        return candidate.id == actionId;
    });
    if (action == match->actions.cend()) {
        return false;
    }

    const QStringList command = expandActionExec(*match, *action);
    if (command.isEmpty()) {
        return false;
    }

    *program = command.constFirst();
    *arguments = command.mid(1);
    return !program->isEmpty();
}

QStringList ApplicationCatalog::defaultApplicationDirectories()
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) {
        dataHome = QDir::home().filePath(QStringLiteral(".local/share"));
    }

    QString dataDirectories = qEnvironmentVariable("XDG_DATA_DIRS");
    if (dataDirectories.isEmpty()) {
        dataDirectories = QStringLiteral("/usr/local/share:/usr/share");
    }

    QStringList result;
    const auto appendApplicationsDirectory = [&result](const QString &basePath) {
        const QString applicationsPath = QDir(basePath).filePath(QStringLiteral("applications"));
        if (!result.contains(applicationsPath)) {
            result.append(applicationsPath);
        }
    };

    appendApplicationsDirectory(dataHome);
    for (const QString &basePath : dataDirectories.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
        appendApplicationsDirectory(basePath);
    }

    return result;
}

bool ApplicationCatalog::readDesktopEntry(const QString &path, const QString &desktopId, DesktopApplication *application)
{
    if (application == nullptr) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QHash<QString, QString> values;
    // Additional actions live in their own groups further down the file, so
    // every group is collected rather than only [Desktop Entry].
    QHash<QString, QHash<QString, QString>> actionGroups;
    QString currentGroup;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.startsWith(QChar::ByteOrderMark)) {
            line.remove(0, 1);
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            currentGroup = trimmed.mid(1, trimmed.size() - 2).trimmed();
            continue;
        }

        const qsizetype equalsPosition = line.indexOf(QLatin1Char('='));
        if (equalsPosition <= 0) {
            continue;
        }

        const QString key = line.left(equalsPosition).trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const QString value = line.mid(equalsPosition + 1).trimmed();

        if (currentGroup == QStringLiteral("Desktop Entry")) {
            values.insert(key, value);
        } else if (currentGroup.startsWith(QStringLiteral("Desktop Action "))) {
            const QString actionId =
                currentGroup.mid(QStringLiteral("Desktop Action ").size()).trimmed();
            if (!actionId.isEmpty()) {
                actionGroups[actionId].insert(key, value);
            }
        }
    }

    if (values.value(QStringLiteral("Type")).trimmed().compare(QStringLiteral("Application"), Qt::CaseInsensitive) != 0
        || desktopBoolean(values.value(QStringLiteral("Hidden")))
        || desktopBoolean(values.value(QStringLiteral("NoDisplay")))) {
        return false;
    }

    const QStringList onlyShowIn = desktopList(values.value(QStringLiteral("OnlyShowIn")));
    const QStringList notShowIn = desktopList(values.value(QStringLiteral("NotShowIn")));
    if ((!onlyShowIn.isEmpty() && !containsNorthstar(onlyShowIn)) || containsNorthstar(notShowIn)) {
        return false;
    }

    DesktopApplication parsed;
    parsed.desktopId = desktopId;
    parsed.name = localizedValue(values, QStringLiteral("Name"));
    parsed.genericName = localizedValue(values, QStringLiteral("GenericName"));
    parsed.exec = values.value(QStringLiteral("Exec")).trimmed();
    parsed.icon = unescapeDesktopValue(values.value(QStringLiteral("Icon"))).trimmed();
    parsed.categories = desktopList(values.value(QStringLiteral("Categories")));
    parsed.mimeTypes = desktopList(values.value(QStringLiteral("MimeType")));
    parsed.sourcePath = path;

    if (parsed.name.isEmpty() || parsed.exec.isEmpty()
        || !tryExecutable(unescapeDesktopValue(values.value(QStringLiteral("TryExec"))))) {
        return false;
    }

    const QStringList command = expandExec(parsed);
    if (command.isEmpty()) {
        return false;
    }

    parsed.launchable = tryExecutable(command.constFirst());

    // Actions are taken in the order the file lists them, because that order
    // is the author's and a menu that reorders them is harder to use twice.
    for (const QString &actionId : desktopList(values.value(QStringLiteral("Actions")))) {
        const auto group = actionGroups.constFind(actionId);
        if (group == actionGroups.constEnd()) {
            continue;
        }

        DesktopAction action;
        action.id = actionId;
        action.name = localizedValue(*group, QStringLiteral("Name"));
        action.exec = group->value(QStringLiteral("Exec")).trimmed();
        action.icon = unescapeDesktopValue(group->value(QStringLiteral("Icon"))).trimmed();

        // An action with nothing to show or nothing to run is not offered.
        // Silently dropping it is right: the application itself is still
        // perfectly usable without it.
        if (action.name.isEmpty() || action.exec.isEmpty()) {
            continue;
        }
        if (expandActionExec(parsed, action).isEmpty()) {
            continue;
        }

        const QStringList onlyShowInAction =
            desktopList(group->value(QStringLiteral("OnlyShowIn")));
        const QStringList notShowInAction = desktopList(group->value(QStringLiteral("NotShowIn")));
        if ((!onlyShowInAction.isEmpty() && !containsNorthstar(onlyShowInAction))
            || containsNorthstar(notShowInAction)) {
            continue;
        }

        parsed.actions.append(action);
    }

    *application = std::move(parsed);
    return true;
}

void ApplicationCatalog::scheduleReload()
{
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start();
    }
}

void ApplicationCatalog::refreshWatchPaths()
{
    const QStringList watchedPaths = m_watcher.directories();
    if (!watchedPaths.isEmpty()) {
        m_watcher.removePaths(watchedPaths);
    }

    QStringList paths;
    for (const QString &directoryPath : std::as_const(m_applicationDirectories)) {
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        paths.append(directory.absolutePath());
        QDirIterator iterator(
            directory.absolutePath(),
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            paths.append(iterator.next());
        }
    }

    paths.removeDuplicates();
    if (!paths.isEmpty()) {
        m_watcher.addPaths(paths);
    }
}

QStringList ApplicationCatalog::tokenizeExec(const QString &exec)
{
    QStringList tokens;
    QString current;
    bool tokenStarted = false;
    bool singleQuoted = false;
    bool doubleQuoted = false;
    bool escaped = false;

    for (const QChar character : exec) {
        if (escaped) {
            current.append(character);
            tokenStarted = true;
            escaped = false;
            continue;
        }

        if (character == QLatin1Char('\\') && !singleQuoted) {
            escaped = true;
            tokenStarted = true;
            continue;
        }

        if (character == QLatin1Char('\'') && !doubleQuoted) {
            singleQuoted = !singleQuoted;
            tokenStarted = true;
            continue;
        }

        if (character == QLatin1Char('"') && !singleQuoted) {
            doubleQuoted = !doubleQuoted;
            tokenStarted = true;
            continue;
        }

        if (character.isSpace() && !singleQuoted && !doubleQuoted) {
            if (tokenStarted) {
                tokens.append(current);
                current.clear();
                tokenStarted = false;
            }
            continue;
        }

        current.append(character);
        tokenStarted = true;
    }

    if (escaped || singleQuoted || doubleQuoted) {
        return {};
    }
    if (tokenStarted) {
        tokens.append(current);
    }
    return tokens;
}

QStringList ApplicationCatalog::expandActionExec(const DesktopApplication &application,
                                                 const DesktopAction &action)
{
    // An action names its own command but borrows the application's identity
    // for the %i and %c field codes, which is what the specification says
    // those codes mean.
    DesktopApplication asApplication = application;
    asApplication.exec = action.exec;
    if (!action.icon.isEmpty()) {
        asApplication.icon = action.icon;
    }
    return expandExec(asApplication);
}

QStringList ApplicationCatalog::expandExec(const DesktopApplication &application)
{
    const QStringList tokens = tokenizeExec(application.exec);
    if (tokens.isEmpty()) {
        return {};
    }

    QStringList command;
    for (const QString &token : tokens) {
        if (token == QStringLiteral("%f") || token == QStringLiteral("%F")
            || token == QStringLiteral("%u") || token == QStringLiteral("%U")) {
            continue;
        }
        if (token == QStringLiteral("%i")) {
            if (!application.icon.isEmpty()) {
                command.append(QStringLiteral("--icon"));
                command.append(application.icon);
            }
            continue;
        }
        if (token == QStringLiteral("%c")) {
            if (!application.name.isEmpty()) {
                command.append(application.name);
            }
            continue;
        }
        if (token == QStringLiteral("%k")) {
            if (!application.sourcePath.isEmpty()) {
                command.append(application.sourcePath);
            }
            continue;
        }

        QString expandedToken;
        for (qsizetype index = 0; index < token.size(); ++index) {
            if (token.at(index) != QLatin1Char('%')) {
                expandedToken.append(token.at(index));
                continue;
            }

            if (index + 1 < token.size() && token.at(index + 1) == QLatin1Char('%')) {
                expandedToken.append(QLatin1Char('%'));
                ++index;
                continue;
            }

            // File and URI field codes cannot be fulfilled without a caller
            // supplying a document. Do not pass unresolved codes onward.
            return {};
        }
        command.append(expandedToken);
    }

    return command;
}
