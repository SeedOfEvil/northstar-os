#include "updateplancontroller.h"

#include "packagetrustcontroller.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace {

constexpr int CurrentSchemaVersion = 1;
constexpr qsizetype MaximumTagLength = 64;
constexpr qsizetype MaximumStringLength = 256;

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isUnresolved(const QString &value)
{
    return value.contains(QStringLiteral("UNSET"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral("RESOLVED_BY_BUILDER"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral("GENERATED_AT_BUILD_TIME"), Qt::CaseInsensitive);
}

bool readString(const QJsonObject &object,
                const QString &key,
                QString &value,
                QString *errorMessage)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        setError(errorMessage, QStringLiteral("key '%1' must be a string").arg(key));
        return false;
    }
    value = jsonValue.toString().trimmed();
    if (value.isEmpty() || isUnresolved(value)) {
        setError(errorMessage, QStringLiteral("key '%1' must be resolved and non-empty").arg(key));
        return false;
    }
    return true;
}

bool hasOnlyKeys(const QJsonObject &object,
                const QSet<QString> &allowedKeys,
                QString *errorMessage,
                const QString &objectName)
{
    for (const QString &key : object.keys()) {
        if (!allowedKeys.contains(key)) {
            setError(errorMessage,
                     QStringLiteral("key '%1' is not part of the %2 contract")
                         .arg(key, objectName));
            return false;
        }
    }
    return true;
}

} // namespace

UpdatePlanController::UpdatePlanController(PackageTrustController *trustController,
                                           QString metadataPath,
                                           QObject *parent)
    : QObject(parent)
    , m_trustController(trustController)
    , m_metadataPath(metadataPath.trimmed())
{
    if (m_metadataPath.isEmpty()) {
        m_metadataPath = defaultMetadataPath();
    }
    reload();
}

bool UpdatePlanController::metadataPresent() const
{
    return m_metadataPresent;
}

bool UpdatePlanController::metadataValid() const
{
    return m_metadataValid;
}

QString UpdatePlanController::signatureStatus() const
{
    return m_signatureStatus;
}

int UpdatePlanController::packageCount() const
{
    return m_metadata.packages.size();
}

int UpdatePlanController::updateCount() const
{
    return m_updateCount;
}

int UpdatePlanController::installCount() const
{
    return m_installCount;
}

int UpdatePlanController::unmanagedCount() const
{
    return m_unmanagedCount;
}

QString UpdatePlanController::metadataPath() const
{
    return m_metadataPath;
}

QString UpdatePlanController::metadataStatus() const
{
    return m_metadataStatus;
}

QString UpdatePlanController::planStatus() const
{
    return m_planStatus;
}

QString UpdatePlanController::planPreview() const
{
    return m_planPreview;
}

bool UpdatePlanController::reload()
{
    m_metadata = {};
    m_signatureStatus.clear();
    m_metadataPresent = QFileInfo::exists(m_metadataPath);
    m_metadataValid = false;
    resetPlan();

    if (!m_metadataPresent) {
        m_metadataStatus = QStringLiteral("No repository publication manifest is configured.");
        setBlockedPlan(QStringLiteral("a valid repository publication manifest is required"));
        emit stateChanged();
        return false;
    }

    QFile file(m_metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_metadataStatus = QStringLiteral("Repository publication manifest could not be read.");
        setBlockedPlan(QStringLiteral("the repository publication manifest could not be read"));
        emit stateChanged();
        return false;
    }

    QString errorMessage;
    if (!parseMetadata(file.readAll(), m_metadata, &errorMessage)) {
        m_metadataStatus = QStringLiteral("Repository publication manifest rejected: %1")
            .arg(errorMessage);
        setBlockedPlan(QStringLiteral("the repository publication manifest was rejected"));
        emit stateChanged();
        return false;
    }

    m_metadataValid = true;
    m_signatureStatus = m_metadata.signatureStatus;
    if (m_signatureStatus == QStringLiteral("verified")) {
        m_metadataStatus = QStringLiteral(
            "Metadata parsed; its verified-signature field is only a claim until Northstar verifies it.");
    } else if (m_signatureStatus == QStringLiteral("rejected")) {
        m_metadataStatus = QStringLiteral("Metadata is explicitly marked as rejected.");
    } else {
        m_metadataStatus = QStringLiteral(
            "Metadata parsed; cryptographic repository signature verification is not connected.");
    }
    setBlockedPlan(QStringLiteral("repository signature verification and update authorization are not connected"));
    emit stateChanged();
    return true;
}

bool UpdatePlanController::preview(const QVariantList &installedPackages)
{
    resetPlan();
    if (!m_metadataValid) {
        setBlockedPlan(QStringLiteral("a valid repository publication manifest is required"));
        emit stateChanged();
        return false;
    }

    if (m_metadata.signatureStatus == QStringLiteral("rejected")) {
        setBlockedPlan(QStringLiteral("the repository publication manifest is marked rejected"));
        emit stateChanged();
        return false;
    }

    if (m_trustController != nullptr) {
        if (!m_trustController->policyValid() || !m_trustController->trustStoreValid()) {
            setBlockedPlan(QStringLiteral("the repository policy and fingerprint store must be valid"));
            emit stateChanged();
            return false;
        }
        if (m_trustController->repositoryTag() != m_metadata.repositoryTag
            || m_trustController->channel() != m_metadata.channel) {
            setBlockedPlan(QStringLiteral("metadata channel and repository tag do not match the trust policy"));
            emit stateChanged();
            return false;
        }
    }

    QHash<QString, QString> installedVersions;
    for (const QVariant &value : installedPackages) {
        const QVariantMap package = value.toMap();
        const QString name = package.value(QStringLiteral("name")).toString().trimmed();
        const QString version = package.value(QStringLiteral("version")).toString().trimmed();
        if (!name.isEmpty() && !installedVersions.contains(name)) {
            installedVersions.insert(name, version);
        }
    }

    for (const RepositoryPackageMetadata &package : m_metadata.packages) {
        if (!installedVersions.contains(package.name)) {
            ++m_installCount;
        } else if (installedVersions.value(package.name) != package.version) {
            ++m_updateCount;
        }
    }

    QSet<QString> managedNames;
    for (const RepositoryPackageMetadata &package : m_metadata.packages) {
        managedNames.insert(package.name);
    }
    for (auto iterator = installedVersions.cbegin(); iterator != installedVersions.cend(); ++iterator) {
        if (!managedNames.contains(iterator.key())) {
            ++m_unmanagedCount;
        }
    }

    m_planPreview = QStringLiteral("Repository revision %1: %2 update candidate%3, %4 new package candidate%5, %6 installed package%7 outside this manifest.")
        .arg(m_metadata.revision)
        .arg(m_updateCount)
        .arg(m_updateCount == 1 ? QString() : QStringLiteral("s"))
        .arg(m_installCount)
        .arg(m_installCount == 1 ? QString() : QStringLiteral("s"))
        .arg(m_unmanagedCount)
        .arg(m_unmanagedCount == 1 ? QString() : QStringLiteral("s"));
    setBlockedPlan(QStringLiteral("preview generated; signature verification and update authorization are not connected"));
    emit stateChanged();
    return true;
}

bool UpdatePlanController::parseMetadata(const QByteArray &contents,
                                         RepositoryMetadata &metadata,
                                         QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(contents, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("metadata must be a JSON object: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject object = document.object();
    if (!hasOnlyKeys(object,
                     {QStringLiteral("schema_version"), QStringLiteral("repository_tag"),
                      QStringLiteral("channel"), QStringLiteral("abi"), QStringLiteral("revision"),
                      QStringLiteral("generated_at"), QStringLiteral("source_revision"),
                      QStringLiteral("signature_status"), QStringLiteral("signature_fingerprint"),
                      QStringLiteral("packages")},
                     errorMessage,
                     QStringLiteral("repository metadata"))) {
        return false;
    }

    if (!object.value(QStringLiteral("schema_version")).isDouble()
        || object.value(QStringLiteral("schema_version")).toInt(-1) != CurrentSchemaVersion) {
        setError(errorMessage, QStringLiteral("schema_version must be %1").arg(CurrentSchemaVersion));
        return false;
    }

    QString repositoryTag;
    QString channel;
    QString abi;
    QString generatedAt;
    QString sourceRevision;
    QString signatureStatus;
    QString signatureFingerprint;
    if (!readString(object, QStringLiteral("repository_tag"), repositoryTag, errorMessage)
        || !readString(object, QStringLiteral("channel"), channel, errorMessage)
        || !readString(object, QStringLiteral("abi"), abi, errorMessage)
        || !readString(object, QStringLiteral("generated_at"), generatedAt, errorMessage)
        || !readString(object, QStringLiteral("source_revision"), sourceRevision, errorMessage)
        || !readString(object, QStringLiteral("signature_status"), signatureStatus, errorMessage)
        || !readString(object, QStringLiteral("signature_fingerprint"), signatureFingerprint, errorMessage)) {
        return false;
    }

    static const QRegularExpression tagPattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]*$"));
    if (repositoryTag.size() > MaximumTagLength || !tagPattern.match(repositoryTag).hasMatch()) {
        setError(errorMessage, QStringLiteral("repository_tag is unsafe"));
        return false;
    }
    if (channel != QStringLiteral("development") && channel != QStringLiteral("stable")) {
        setError(errorMessage, QStringLiteral("channel must be development or stable"));
        return false;
    }
    if (!QRegularExpression(QStringLiteral("^FreeBSD:[0-9]+:[A-Za-z0-9_]+$")).match(abi).hasMatch()) {
        setError(errorMessage, QStringLiteral("abi must identify a FreeBSD release and architecture"));
        return false;
    }
    if (!QDateTime::fromString(generatedAt, Qt::ISODate).isValid()) {
        setError(errorMessage, QStringLiteral("generated_at must be an ISO-8601 timestamp"));
        return false;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{7,64}$")).match(sourceRevision).hasMatch()) {
        setError(errorMessage, QStringLiteral("source_revision must be a resolved commit identifier"));
        return false;
    }
    if (signatureStatus != QStringLiteral("unverified")
        && signatureStatus != QStringLiteral("verified")
        && signatureStatus != QStringLiteral("rejected")) {
        setError(errorMessage, QStringLiteral("signature_status must be unverified, verified, or rejected"));
        return false;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{64}$")).match(signatureFingerprint).hasMatch()) {
        setError(errorMessage, QStringLiteral("signature_fingerprint must contain 64 hexadecimal characters"));
        return false;
    }

    const QJsonValue revisionValue = object.value(QStringLiteral("revision"));
    const int revision = revisionValue.toInt(-1);
    if (!revisionValue.isDouble() || revision < 0) {
        setError(errorMessage, QStringLiteral("revision must be a non-negative integer"));
        return false;
    }

    const QJsonValue packagesValue = object.value(QStringLiteral("packages"));
    if (!packagesValue.isArray() || packagesValue.toArray().isEmpty()) {
        setError(errorMessage, QStringLiteral("packages must be a non-empty array"));
        return false;
    }

    QList<RepositoryPackageMetadata> packages;
    QSet<QString> packageNames;
    const QSet<QString> allowedPackageKeys{
        QStringLiteral("name"), QStringLiteral("version"), QStringLiteral("origin"),
        QStringLiteral("source"), QStringLiteral("project_revision")};
    for (const QJsonValue &packageValue : packagesValue.toArray()) {
        if (!packageValue.isObject()) {
            setError(errorMessage, QStringLiteral("every package entry must be an object"));
            return false;
        }
        const QJsonObject packageObject = packageValue.toObject();
        if (!hasOnlyKeys(packageObject, allowedPackageKeys, errorMessage, QStringLiteral("package metadata"))) {
            return false;
        }

        RepositoryPackageMetadata package;
        if (!readString(packageObject, QStringLiteral("name"), package.name, errorMessage)
            || !readString(packageObject, QStringLiteral("version"), package.version, errorMessage)
            || !readString(packageObject, QStringLiteral("origin"), package.origin, errorMessage)
            || !readString(packageObject, QStringLiteral("source"), package.source, errorMessage)
            || !readString(packageObject, QStringLiteral("project_revision"), package.projectRevision, errorMessage)) {
            return false;
        }

        if (package.name.size() > MaximumStringLength
            || !QRegularExpression(QStringLiteral("^[a-z0-9][a-z0-9+_.-]*$")).match(package.name).hasMatch()) {
            setError(errorMessage, QStringLiteral("package name '%1' is unsafe").arg(package.name));
            return false;
        }
        if (package.version.size() > MaximumStringLength
            || package.version.contains(QRegularExpression(QStringLiteral("\\s")))) {
            setError(errorMessage, QStringLiteral("package '%1' has an unsafe version").arg(package.name));
            return false;
        }
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_.+~-]+/[A-Za-z0-9_.+~-]+$")).match(package.origin).hasMatch()) {
            setError(errorMessage, QStringLiteral("package '%1' has an unsafe origin").arg(package.name));
            return false;
        }
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_.+/-]+$")).match(package.source).hasMatch()
            || package.source.contains(QStringLiteral(".."))) {
            setError(errorMessage, QStringLiteral("package '%1' has an unsafe source").arg(package.name));
            return false;
        }
        if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{7,64}$")).match(package.projectRevision).hasMatch()) {
            setError(errorMessage, QStringLiteral("package '%1' has an unresolved project revision").arg(package.name));
            return false;
        }
        if (packageNames.contains(package.name)) {
            setError(errorMessage, QStringLiteral("package '%1' is duplicated").arg(package.name));
            return false;
        }
        packageNames.insert(package.name);
        packages.append(std::move(package));
    }

    metadata = RepositoryMetadata{
        CurrentSchemaVersion,
        repositoryTag,
        channel,
        abi,
        revision,
        generatedAt,
        sourceRevision,
        signatureStatus,
        signatureFingerprint.toLower(),
        packages,
    };
    return true;
}

QString UpdatePlanController::defaultMetadataPath()
{
    const QString userConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/repository-metadata.json");
    if (QFileInfo::exists(userConfigPath)) {
        return userConfigPath;
    }

    const QString installedPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/repository-metadata.json"),
        QStandardPaths::LocateFile);
    return installedPath.isEmpty() ? userConfigPath : installedPath;
}

void UpdatePlanController::resetPlan()
{
    m_updateCount = 0;
    m_installCount = 0;
    m_unmanagedCount = 0;
    m_planPreview.clear();
}

void UpdatePlanController::setBlockedPlan(const QString &reason)
{
    m_planStatus = QStringLiteral("Update planning is blocked: %1.").arg(reason);
}
