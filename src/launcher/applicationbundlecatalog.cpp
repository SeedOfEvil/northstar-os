#include "applicationbundlecatalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QMetaType>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>
#include <QXmlStreamReader>

#include <algorithm>
#include <utility>

namespace {

struct BundleManifest
{
    QString bundleId;
    QString displayName;
    QString version;
    QString executable;
    QString icon;
    QStringList categories;
    BundleProvenance provenance;
};

QVariant readPlistValue(QXmlStreamReader &reader, bool *ok)
{
    if (ok == nullptr || reader.tokenType() != QXmlStreamReader::StartElement) {
        if (ok != nullptr) {
            *ok = false;
        }
        return {};
    }

    const QString elementName = reader.name().toString();
    if (elementName == QStringLiteral("string")) {
        return reader.readElementText(QXmlStreamReader::SkipChildElements);
    }

    if (elementName == QStringLiteral("integer")) {
        bool converted = false;
        const qlonglong value = reader.readElementText(QXmlStreamReader::SkipChildElements).toLongLong(&converted);
        if (!converted) {
            *ok = false;
            return {};
        }
        return value;
    }

    if (elementName == QStringLiteral("real")) {
        bool converted = false;
        const double value = reader.readElementText(QXmlStreamReader::SkipChildElements).toDouble(&converted);
        if (!converted) {
            *ok = false;
            return {};
        }
        return value;
    }

    if (elementName == QStringLiteral("true") || elementName == QStringLiteral("false")) {
        const bool value = elementName == QStringLiteral("true");
        reader.skipCurrentElement();
        return value;
    }

    if (elementName == QStringLiteral("array")) {
        QVariantList values;
        while (reader.readNextStartElement()) {
            bool childOk = true;
            values.append(readPlistValue(reader, &childOk));
            if (!childOk) {
                *ok = false;
                return {};
            }
        }
        return values;
    }

    if (elementName == QStringLiteral("dict")) {
        QVariantMap values;
        while (reader.readNextStartElement()) {
            if (reader.name() != QStringLiteral("key")) {
                *ok = false;
                reader.skipCurrentElement();
                return {};
            }

            const QString key = reader.readElementText(QXmlStreamReader::SkipChildElements);
            if (key.isEmpty() || !reader.readNextStartElement()) {
                *ok = false;
                return {};
            }

            bool childOk = true;
            const QVariant value = readPlistValue(reader, &childOk);
            if (!childOk || values.contains(key)) {
                *ok = false;
                return {};
            }
            values.insert(key, value);
        }
        return values;
    }

    // Unknown keys are allowed by the format. Consume an unknown value so
    // older launchers can ignore future manifest fields safely.
    reader.skipCurrentElement();
    return {};
}

bool validProvenanceValue(const QString &value)
{
    if (value.isEmpty() || value.size() > 200 || value != value.trimmed()) {
        return false;
    }

    for (const QChar character : value) {
        if (character.isNull() || character == QLatin1Char('\n')
            || character == QLatin1Char('\r') || character == QLatin1Char('\t')) {
            return false;
        }
    }
    return true;
}

bool readManifest(const QString &path, BundleManifest *manifest)
{
    if (manifest == nullptr) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QXmlStreamReader reader(&file);
    if (!reader.readNextStartElement() || reader.name() != QStringLiteral("plist")) {
        return false;
    }

    QVariantMap values;
    bool foundDictionary = false;
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("dict") || foundDictionary) {
            reader.skipCurrentElement();
            continue;
        }

        bool dictionaryOk = true;
        const QVariant dictionary = readPlistValue(reader, &dictionaryOk);
        if (!dictionaryOk || dictionary.typeId() != QMetaType::QVariantMap) {
            return false;
        }
        values = dictionary.toMap();
        foundDictionary = true;
    }

    if (reader.hasError() || !foundDictionary) {
        return false;
    }

    const auto stringValue = [&values](const QString &key, QString *output) {
        const QVariant value = values.value(key);
        if (!value.isValid() || value.typeId() != QMetaType::QString || value.toString().isEmpty()) {
            return false;
        }
        *output = value.toString();
        return true;
    };

    BundleManifest parsed;
    if (!stringValue(QStringLiteral("BundleIdentifier"), &parsed.bundleId)
        || !stringValue(QStringLiteral("DisplayName"), &parsed.displayName)
        || !stringValue(QStringLiteral("Version"), &parsed.version)
        || !stringValue(QStringLiteral("Executable"), &parsed.executable)
        || !stringValue(QStringLiteral("Icon"), &parsed.icon)) {
        return false;
    }

    const QVariant categoriesValue = values.value(QStringLiteral("Categories"));
    if (categoriesValue.typeId() != QMetaType::QVariantList) {
        return false;
    }
    for (const QVariant &category : categoriesValue.toList()) {
        if (category.typeId() != QMetaType::QString || category.toString().isEmpty()) {
            return false;
        }
        parsed.categories.append(category.toString());
    }
    if (parsed.categories.isEmpty()) {
        return false;
    }

    const QVariant provenanceValue = values.value(QStringLiteral("Provenance"));
    if (provenanceValue.typeId() != QMetaType::QVariantMap) {
        return false;
    }

    const QVariantMap provenanceValues = provenanceValue.toMap();
    const auto provenanceStringValue = [&provenanceValues](const QString &key, QString *output) {
        const QVariant value = provenanceValues.value(key);
        if (!value.isValid() || value.typeId() != QMetaType::QString
            || !validProvenanceValue(value.toString())) {
            return false;
        }
        *output = value.toString();
        return true;
    };

    if (!provenanceStringValue(QStringLiteral("Source"), &parsed.provenance.source)
        || !provenanceStringValue(QStringLiteral("Package"), &parsed.provenance.package)
        || !provenanceStringValue(QStringLiteral("Revision"), &parsed.provenance.revision)) {
        return false;
    }

    *manifest = std::move(parsed);
    return true;
}

bool validBundleIdentifier(const QString &bundleId)
{
    if (bundleId.isEmpty() || bundleId.size() > 200) {
        return false;
    }

    for (const QChar character : bundleId) {
        if (!(character.isLetterOrNumber()
              || character == QLatin1Char('.')
              || character == QLatin1Char('-')
              || character == QLatin1Char('_'))) {
            return false;
        }
    }
    return true;
}

bool validRelativePath(const QString &path)
{
    if (path.isEmpty() || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char(':'))
        || QDir::isAbsolutePath(path) || path.startsWith(QLatin1Char('/'))
        || path.endsWith(QLatin1Char('/'))) {
        return false;
    }

    const QStringList components = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    if (components.isEmpty()) {
        return false;
    }
    for (const QString &component : components) {
        if (component.isEmpty() || component == QStringLiteral(".")
            || component == QStringLiteral("..")) {
            return false;
        }
    }
    return true;
}

bool hasSymlinkComponent(const QString &basePath, const QString &relativePath)
{
    QString currentPath = basePath;
    for (const QString &component : relativePath.split(QLatin1Char('/'))) {
        currentPath = QDir(currentPath).filePath(component);
        if (QFileInfo(currentPath).isSymLink()) {
            return true;
        }
    }
    return false;
}

bool isOwnedAndNotGroupWritable(const QFileInfo &info, uint ownerId)
{
    if (!info.exists() || info.ownerId() != ownerId) {
        return false;
    }

    const QFileDevice::Permissions unsafePermissions = QFileDevice::WriteGroup
        | QFileDevice::WriteOther;
    return !(info.permissions() & unsafePermissions);
}

bool resolveOwnedFile(const QString &basePath,
                      const QString &relativePath,
                      uint ownerId,
                      QFileInfo *resolved)
{
    if (resolved == nullptr || !validRelativePath(relativePath) || hasSymlinkComponent(basePath, relativePath)) {
        return false;
    }

    const QFileInfo baseInfo(basePath);
    if (!baseInfo.isDir() || baseInfo.isSymLink()) {
        return false;
    }

    const QString canonicalBase = baseInfo.canonicalFilePath();
    const QString candidatePath = QDir(basePath).filePath(relativePath);
    const QFileInfo candidateInfo(candidatePath);
    if (!candidateInfo.isFile() || candidateInfo.isSymLink()
        || !isOwnedAndNotGroupWritable(candidateInfo, ownerId)) {
        return false;
    }

    const QString canonicalCandidate = candidateInfo.canonicalFilePath();
    if (canonicalBase.isEmpty() || canonicalCandidate.isEmpty()
        || (canonicalCandidate != canonicalBase
            && !canonicalCandidate.startsWith(canonicalBase + QLatin1Char('/')))) {
        return false;
    }

    *resolved = candidateInfo;
    return true;
}

bool readBundle(const QString &path, BundleApplication *application)
{
    if (application == nullptr) {
        return false;
    }

    const QFileInfo bundleInfo(path);
    if (!bundleInfo.isDir() || bundleInfo.isSymLink()
        || !bundleInfo.fileName().endsWith(QStringLiteral(".app"), Qt::CaseSensitive)) {
        return false;
    }

    const QString bundlePath = bundleInfo.canonicalFilePath();
    if (bundlePath.isEmpty() || !isOwnedAndNotGroupWritable(bundleInfo, bundleInfo.ownerId())) {
        return false;
    }

    const uint ownerId = bundleInfo.ownerId();
    const QString contentsPath = QDir(bundlePath).filePath(QStringLiteral("Contents"));
    const QFileInfo contentsInfo(contentsPath);
    if (!contentsInfo.isDir() || contentsInfo.isSymLink()
        || !isOwnedAndNotGroupWritable(contentsInfo, ownerId)) {
        return false;
    }

    const QString manifestPath = QDir(contentsPath).filePath(QStringLiteral("Info.plist"));
    const QFileInfo manifestInfo(manifestPath);
    if (manifestInfo.isSymLink() || !isOwnedAndNotGroupWritable(manifestInfo, ownerId)) {
        return false;
    }

    BundleManifest manifest;
    if (!readManifest(manifestPath, &manifest) || !validBundleIdentifier(manifest.bundleId)) {
        return false;
    }

    const QString executableRoot = QDir(contentsPath).filePath(QStringLiteral("Executable"));
    const QString resourcesRoot = QDir(contentsPath).filePath(QStringLiteral("Resources"));
    const QFileInfo executableRootInfo(executableRoot);
    const QFileInfo resourcesRootInfo(resourcesRoot);
    if (!executableRootInfo.isDir() || executableRootInfo.isSymLink()
        || !resourcesRootInfo.isDir() || resourcesRootInfo.isSymLink()
        || !isOwnedAndNotGroupWritable(executableRootInfo, ownerId)
        || !isOwnedAndNotGroupWritable(resourcesRootInfo, ownerId)) {
        return false;
    }

    QFileInfo executableInfo;
    QFileInfo iconInfo;
    if (!resolveOwnedFile(executableRoot, manifest.executable, ownerId, &executableInfo)
        || !executableInfo.isExecutable()
        || !resolveOwnedFile(resourcesRoot, manifest.icon, ownerId, &iconInfo)) {
        return false;
    }

    BundleApplication parsed;
    parsed.bundleId = manifest.bundleId;
    parsed.desktopId = QStringLiteral("bundle:") + manifest.bundleId;
    parsed.name = manifest.displayName;
    parsed.version = manifest.version;
    parsed.executable = manifest.executable;
    parsed.icon = manifest.icon;
    parsed.categories = manifest.categories;
    parsed.bundlePath = bundlePath;
    parsed.executablePath = executableInfo.canonicalFilePath();
    parsed.iconPath = iconInfo.canonicalFilePath();
    parsed.provenance = manifest.provenance;
    *application = std::move(parsed);
    return true;
}

bool matchesQuery(const BundleApplication &application, const QStringList &terms)
{
    const QString searchText = QStringList{
        application.name,
        application.version,
        application.bundleId,
        application.categories.join(QLatin1Char(' ')),
        application.provenance.source,
        application.provenance.package,
        application.provenance.revision
    }.join(QLatin1Char(' '));

    return std::all_of(terms.cbegin(), terms.cend(), [&searchText](const QString &term) {
        return searchText.contains(term, Qt::CaseInsensitive);
    });
}

} // namespace

ApplicationBundleCatalog::ApplicationBundleCatalog(QStringList bundleDirectories, QObject *parent)
    : QObject(parent)
    , m_bundleDirectories(std::move(bundleDirectories))
    , m_watcher(this)
    , m_refreshTimer(this)
{
    if (m_bundleDirectories.isEmpty()) {
        m_bundleDirectories = defaultBundleDirectories();
    }

    m_refreshTimer.setInterval(150);
    m_refreshTimer.setSingleShot(true);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &ApplicationBundleCatalog::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ApplicationBundleCatalog::scheduleReload);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        reload();
    });

    reload();
}

QList<BundleApplication> ApplicationBundleCatalog::entries() const
{
    return m_entries;
}

QVariantList ApplicationBundleCatalog::applications() const
{
    return toVariantList(m_entries);
}

QVariantList ApplicationBundleCatalog::searchApplications(const QString &query) const
{
    const QStringList terms = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        return applications();
    }

    QList<BundleApplication> matches;
    for (const BundleApplication &application : m_entries) {
        if (matchesQuery(application, terms)) {
            matches.append(application);
        }
    }
    return toVariantList(matches);
}

QVariantList ApplicationBundleCatalog::toVariantList(const QList<BundleApplication> &entries)
{
    QVariantList result;
    result.reserve(entries.size());

    for (const BundleApplication &application : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("desktopId"), application.desktopId);
        item.insert(QStringLiteral("name"), application.name);
        item.insert(QStringLiteral("genericName"), QStringLiteral("Version ") + application.version);
        item.insert(QStringLiteral("version"), application.version);
        item.insert(QStringLiteral("icon"), application.icon);
        item.insert(QStringLiteral("iconSource"), QUrl::fromLocalFile(application.iconPath));
        item.insert(QStringLiteral("categories"), application.categories);
        item.insert(QStringLiteral("sourceType"), QStringLiteral("bundle"));
        item.insert(QStringLiteral("provenanceSource"), application.provenance.source);
        item.insert(QStringLiteral("provenancePackage"), application.provenance.package);
        item.insert(QStringLiteral("provenanceRevision"), application.provenance.revision);
        result.append(item);
    }

    return result;
}

QStringList ApplicationBundleCatalog::applicationIds() const
{
    QStringList result;
    result.reserve(m_entries.size());
    for (const BundleApplication &application : m_entries) {
        result.append(application.desktopId);
    }
    return result;
}

bool ApplicationBundleCatalog::reload()
{
    QList<BundleApplication> discovered;
    QSet<QString> seenBundleIds;

    for (const QString &directoryPath : std::as_const(m_bundleDirectories)) {
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        const QFileInfoList candidates = directory.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &candidate : candidates) {
            if (!candidate.fileName().endsWith(QStringLiteral(".app"), Qt::CaseSensitive)) {
                continue;
            }

            BundleApplication application;
            if (!readBundle(candidate.absoluteFilePath(), &application)
                || seenBundleIds.contains(application.bundleId)) {
                continue;
            }
            seenBundleIds.insert(application.bundleId);
            discovered.append(std::move(application));
        }
    }

    std::sort(discovered.begin(), discovered.end(), [](const BundleApplication &left, const BundleApplication &right) {
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

void ApplicationBundleCatalog::scheduleReload()
{
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start();
    }
}

void ApplicationBundleCatalog::refreshWatchPaths()
{
    const QStringList watchedPaths = m_watcher.directories();
    if (!watchedPaths.isEmpty()) {
        m_watcher.removePaths(watchedPaths);
    }

    QStringList paths;
    for (const QString &directoryPath : std::as_const(m_bundleDirectories)) {
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

bool ApplicationBundleCatalog::launchSpec(const QString &desktopId,
                                          QString *program,
                                          QStringList *arguments) const
{
    if (program == nullptr || arguments == nullptr) {
        return false;
    }

    const auto match = std::find_if(m_entries.cbegin(), m_entries.cend(), [&desktopId](const BundleApplication &application) {
        return application.desktopId == desktopId;
    });
    if (match == m_entries.cend()) {
        return false;
    }

    *program = match->executablePath;
    arguments->clear();
    return !program->isEmpty();
}

QStringList ApplicationBundleCatalog::defaultBundleDirectories()
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) {
        dataHome = QDir::home().filePath(QStringLiteral(".local/share"));
    }

    QStringList result;
    const auto appendRoot = [&result](const QString &path) {
        const QString bundlePath = QDir(path).filePath(QStringLiteral("northstar/apps"));
        if (!result.contains(bundlePath)) {
            result.append(bundlePath);
        }
    };

    appendRoot(dataHome);
    appendRoot(QStringLiteral("/usr/local/share"));
    appendRoot(QStringLiteral("/usr/share"));
    return result;
}
