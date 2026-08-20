#include "updateplancontroller.h"

#include "packagetrustcontroller.h"

#include <QDateTime>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariantMap>

#include <utility>

namespace {

constexpr int CurrentSchemaVersion = 1;
constexpr int CurrentSignatureSchemaVersion = 2;
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
                                           QObject *parent,
                                           QString repositoryConfigDirectory)
    : QObject(parent)
    , m_trustController(trustController)
    , m_metadataPath(metadataPath.trimmed())
    , m_repositoryConfigDirectory(repositoryConfigDirectory.trimmed())
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

bool UpdatePlanController::cataloguePresent() const
{
    return m_cataloguePresent;
}

bool UpdatePlanController::catalogueDigestValid() const
{
    return m_catalogueDigestValid;
}

bool UpdatePlanController::signatureVerified() const
{
    return m_signatureVerified;
}

bool UpdatePlanController::previewReady() const
{
    return m_previewReady;
}

QString UpdatePlanController::signatureStatus() const
{
    return m_signatureStatus;
}

QString UpdatePlanController::signatureFingerprint() const
{
    return m_metadata.signatureFingerprint;
}

QString UpdatePlanController::catalogueSha256() const
{
    return m_metadata.catalogueSha256;
}

QString UpdatePlanController::publicationManifestSha256() const
{
    return m_publicationManifestSha256;
}

QString UpdatePlanController::repositoryTag() const
{
    return m_metadata.repositoryTag;
}

QString UpdatePlanController::channel() const
{
    return m_metadata.channel;
}

QString UpdatePlanController::abi() const
{
    return m_metadata.abi;
}

QString UpdatePlanController::generatedAt() const
{
    return m_metadata.generatedAt;
}

QVariantList UpdatePlanController::packageProvenance() const
{
    QVariantList values;
    values.reserve(m_metadata.packages.size());
    for (const RepositoryPackageMetadata &package : m_metadata.packages) {
        values.append(QVariantMap{
            {QStringLiteral("name"), package.name},
            {QStringLiteral("version"), package.version},
            {QStringLiteral("origin"), package.origin},
            {QStringLiteral("source"), package.source},
            {QStringLiteral("projectRevision"), package.projectRevision},
        });
    }
    return values;
}

int UpdatePlanController::repositoryRevision() const
{
    return m_metadata.revision;
}

QString UpdatePlanController::sourceRevision() const
{
    return m_metadata.sourceRevision;
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

int UpdatePlanController::publishedRevision() const
{
    return m_publishedRevision;
}

QString UpdatePlanController::blockedReason() const
{
    return m_blockedReason;
}

QString UpdatePlanController::activeRepositoryPath(const QString &repositoryConfigDirectory)
{
    const QString directory = repositoryConfigDirectory.trimmed().isEmpty()
        ? QStringLiteral("/usr/local/etc/pkg/repos")
        : repositoryConfigDirectory.trimmed();

    QDir repositories(directory);
    if (!repositories.exists()) {
        return QString();
    }

    // Only a local repository can be read without fetching anything, which
    // is the whole point: this reports on what is already on the machine.
    static const QRegularExpression localUrl(
        QStringLiteral(R"RE(url\s*:\s*"file://([^"]+)")RE"));
    for (const QString &name : repositories.entryList({QStringLiteral("*.conf")}, QDir::Files)) {
        QFile file(repositories.filePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString contents = QString::fromUtf8(file.read(64 * 1024));
        file.close();

        if (!contents.contains(QStringLiteral("northstar"))) {
            continue;
        }
        const QRegularExpressionMatch match = localUrl.match(contents);
        if (match.hasMatch()) {
            return QDir::cleanPath(match.captured(1));
        }
    }
    return QString();
}

// The revision the repository itself claims to be, taken from the publication
// record it ships. Read separately from the trust material so the two can be
// compared rather than assumed equal.
int UpdatePlanController::readPublishedRevision() const
{
    const QString repository = activeRepositoryPath(m_repositoryConfigDirectory);
    if (repository.isEmpty()) {
        return 0;
    }

    QFile record(QDir(repository).filePath(QStringLiteral("publication-record.conf")));
    if (!record.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    const QString contents = QString::fromUtf8(record.read(64 * 1024));
    record.close();

    for (const QString &line : contents.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1String("repository_revision="))) {
            continue;
        }
        bool numeric = false;
        const int revision =
            trimmed.mid(QStringLiteral("repository_revision=").size()).trimmed().toInt(&numeric);
        return numeric && revision > 0 ? revision : 0;
    }
    return 0;
}

QString UpdatePlanController::metadataPath() const
{
    return m_metadataPath;
}

QString UpdatePlanController::catalogueFile() const
{
    return m_catalogueFile;
}

QString UpdatePlanController::catalogueStatus() const
{
    return m_catalogueStatus;
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
    m_publicationManifestSha256.clear();
    m_catalogueFile.clear();
    m_catalogueStatus.clear();
    m_metadataPresent = QFileInfo::exists(m_metadataPath);
    m_metadataValid = false;
    m_cataloguePresent = false;
    m_catalogueDigestValid = false;
    m_signatureVerified = false;
    m_previewReady = false;
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

    const QByteArray metadataBytes = file.readAll();
    m_publicationManifestSha256 = QString::fromLatin1(
        QCryptographicHash::hash(metadataBytes, QCryptographicHash::Sha256).toHex());
    QString errorMessage;
    if (!parseMetadata(metadataBytes, m_metadata, &errorMessage)) {
        m_metadataStatus = QStringLiteral("Repository publication manifest rejected: %1")
            .arg(errorMessage);
        setBlockedPlan(QStringLiteral("the repository publication manifest was rejected"));
        emit stateChanged();
        return false;
    }

    m_signatureStatus = m_metadata.signatureStatus;
    m_catalogueFile = m_metadata.catalogueFile;
    QString catalogueError;
    if (!verifyCatalogueIntegrity(&catalogueError)) {
        m_metadataStatus = QStringLiteral("Repository publication manifest rejected: %1")
            .arg(catalogueError);
        setBlockedPlan(QStringLiteral("the repository catalogue failed its content-addressed integrity check"));
        emit stateChanged();
        return false;
    }

    m_metadataValid = true;
    QString signatureError;
    m_signatureVerified = verifySignature(&signatureError);
    if (m_signatureVerified) {
        m_signatureStatus = QStringLiteral("verified");
        m_metadataStatus = QStringLiteral(
            "Catalogue digest and publication signature verified; update authorization is not connected.");
    } else {
        m_metadataStatus = QStringLiteral(
            "Catalogue digest verified, but publication signature is not verified: %1")
            .arg(signatureError);
    }
    setBlockedPlan(m_signatureVerified
        ? QStringLiteral("update authorization is not connected")
        : QStringLiteral("the publication signature is not verified"));

    m_publishedRevision = readPublishedRevision();
    if (m_signatureVerified) {
        m_blockedReason = QStringLiteral(
            "Updates are ready to plan once update authorization is connected.");
    } else if (m_publishedRevision > 0 && m_publishedRevision != m_metadata.revision) {
        // The common case, and the one a person can do something about: the
        // trust material on this system was installed for an older revision
        // than the repository is now publishing, so its signature cannot
        // match. Saying which two numbers disagree is the whole difference
        // between an actionable message and a wall of cryptography.
        m_blockedReason = QStringLiteral(
            "This system trusts revision %1, but the repository publishes revision %2. "
            "Updates stay blocked until the trust material for revision %2 is installed.")
            .arg(m_metadata.revision)
            .arg(m_publishedRevision);
    } else {
        m_blockedReason = QStringLiteral(
            "The repository publication is not signed by a key this system trusts.");
    }

    emit stateChanged();
    return true;
}

bool UpdatePlanController::preview(const QVariantList &installedPackages)
{
    resetPlan();
    if (!m_metadataValid) {
        setBlockedPlan(QStringLiteral("a valid publication manifest and catalogue digest are required"));
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
    m_previewReady = true;
    setBlockedPlan(m_signatureVerified
        ? QStringLiteral("preview generated; update authorization is not connected")
        : QStringLiteral("preview generated; publication signature is not verified"));
    emit stateChanged();
    return true;
}

bool UpdatePlanController::verifySignature(QString *errorMessage)
{
    if (m_metadata.signatureStatus == QStringLiteral("rejected")) {
        setError(errorMessage, QStringLiteral("the publication manifest is marked rejected"));
        return false;
    }
    if (m_trustController == nullptr
        || !m_trustController->policyValid()
        || !m_trustController->trustStoreValid()) {
        setError(errorMessage, QStringLiteral("the repository policy and fingerprint store are not valid"));
        return false;
    }

    const QFileInfo metadataInfo(m_metadataPath);
    const QFileInfo envelopeInfo(
        QDir(metadataInfo.absolutePath()).filePath(m_metadata.signatureEnvelope));
    if (!envelopeInfo.exists() || !envelopeInfo.isFile() || envelopeInfo.isSymLink()) {
        setError(errorMessage, QStringLiteral("the publication signature envelope is missing or unsafe"));
        return false;
    }

    QFile envelopeFile(envelopeInfo.absoluteFilePath());
    if (!envelopeFile.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("the publication signature envelope could not be read"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(envelopeFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("the publication signature envelope is not valid JSON"));
        return false;
    }

    const QJsonObject object = document.object();
    if (!hasOnlyKeys(object,
                     {QStringLiteral("schema_version"), QStringLiteral("type"),
                      QStringLiteral("payload_type"), QStringLiteral("payload"), QStringLiteral("public_key_pem"),
                      QStringLiteral("signature_base64"), QStringLiteral("fingerprint_sha256")},
                     errorMessage,
                     QStringLiteral("publication signature"))) {
        return false;
    }
    if (!object.value(QStringLiteral("schema_version")).isDouble()) {
        setError(errorMessage, QStringLiteral("signature schema_version must be an integer"));
        return false;
    }
    const int signatureSchemaVersion = object.value(QStringLiteral("schema_version")).toInt(-1);
    if (signatureSchemaVersion != CurrentSchemaVersion
        && signatureSchemaVersion != CurrentSignatureSchemaVersion) {
        setError(errorMessage, QStringLiteral("signature schema_version must be 1 or %1")
            .arg(CurrentSignatureSchemaVersion));
        return false;
    }

    QString type;
    QString payload;
    QString publicKeyPem;
    QString signatureBase64;
    QString fingerprint;
    if (!readString(object, QStringLiteral("type"), type, errorMessage)
        || !readString(object, QStringLiteral("payload"), payload, errorMessage)
        || !readString(object, QStringLiteral("signature_base64"), signatureBase64, errorMessage)
        || !readString(object, QStringLiteral("fingerprint_sha256"), fingerprint, errorMessage)) {
        return false;
    }
    const QJsonValue publicKeyValue = object.value(QStringLiteral("public_key_pem"));
    if (!publicKeyValue.isString()) {
        setError(errorMessage, QStringLiteral("key 'public_key_pem' must be a string"));
        return false;
    }
    publicKeyPem = publicKeyValue.toString();
    if (publicKeyPem.trimmed().isEmpty() || isUnresolved(publicKeyPem)) {
        setError(errorMessage, QStringLiteral("key 'public_key_pem' must be resolved and non-empty"));
        return false;
    }
    if (type != QStringLiteral("rsa")) {
        setError(errorMessage, QStringLiteral("only rsa publication signatures are supported"));
        return false;
    }
    QString expectedPayload = m_metadata.catalogueSha256;
    if (signatureSchemaVersion == CurrentSignatureSchemaVersion) {
        QString payloadType;
        if (!readString(object, QStringLiteral("payload_type"), payloadType, errorMessage)) {
            return false;
        }
        if (payloadType != QStringLiteral("repository-metadata-sha256")) {
            setError(errorMessage, QStringLiteral("signature payload_type must bind repository metadata"));
            return false;
        }
        expectedPayload = m_publicationManifestSha256;
    } else if (object.contains(QStringLiteral("payload_type"))) {
        setError(errorMessage, QStringLiteral("signature schema_version 1 must not declare payload_type"));
        return false;
    }
    if (payload != expectedPayload) {
        setError(errorMessage, signatureSchemaVersion == CurrentSignatureSchemaVersion
            ? QStringLiteral("signature payload does not match the publication manifest digest")
            : QStringLiteral("signature payload does not match the catalogue digest"));
        return false;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{64}$")).match(fingerprint).hasMatch()) {
        setError(errorMessage, QStringLiteral("signature fingerprint must contain 64 hexadecimal characters"));
        return false;
    }
    fingerprint = fingerprint.toLower();
    if (fingerprint != m_metadata.signatureFingerprint) {
        setError(errorMessage, QStringLiteral("signature fingerprint does not match the publication manifest"));
        return false;
    }
    if (!publicKeyPem.contains(QStringLiteral("BEGIN PUBLIC KEY"))) {
        setError(errorMessage, QStringLiteral("publication signature does not contain a PEM public key"));
        return false;
    }
    const QString actualFingerprint = QString::fromLatin1(
        QCryptographicHash::hash(publicKeyPem.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (actualFingerprint != fingerprint) {
        setError(errorMessage, QStringLiteral("public-key fingerprint does not match the envelope"));
        return false;
    }
    if (!m_trustController->hasTrustedFingerprint(fingerprint)) {
        setError(errorMessage, QStringLiteral("public-key fingerprint is not trusted for this repository"));
        return false;
    }

    const QByteArray signature = QByteArray::fromBase64(signatureBase64.toLatin1());
    if (signature.isEmpty()) {
        setError(errorMessage, QStringLiteral("publication signature is empty or not valid base64"));
        return false;
    }

    const QString opensslPath = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (opensslPath.isEmpty()) {
        setError(errorMessage, QStringLiteral("openssl is unavailable for signature verification"));
        return false;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        setError(errorMessage, QStringLiteral("temporary signature-verification storage is unavailable"));
        return false;
    }
    const QString payloadPath = QDir(directory.path()).filePath(QStringLiteral("payload"));
    const QString publicKeyPath = QDir(directory.path()).filePath(QStringLiteral("public-key.pem"));
    const QString signaturePath = QDir(directory.path()).filePath(QStringLiteral("signature.bin"));
    const auto writeBytes = [](const QString &path, const QByteArray &bytes) -> bool {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    };
    if (!writeBytes(payloadPath, payload.toUtf8())
        || !writeBytes(publicKeyPath, publicKeyPem.toUtf8())
        || !writeBytes(signaturePath, signature)) {
        setError(errorMessage, QStringLiteral("temporary signature-verification files could not be written"));
        return false;
    }

    QProcess verifier;
    verifier.setProgram(opensslPath);
    verifier.setArguments({QStringLiteral("dgst"), QStringLiteral("-sha256"),
                           QStringLiteral("-verify"), publicKeyPath,
                           QStringLiteral("-signature"), signaturePath, payloadPath});
    verifier.start();
    if (!verifier.waitForStarted(1500) || !verifier.waitForFinished(5000)) {
        verifier.kill();
        verifier.waitForFinished(1000);
        setError(errorMessage, QStringLiteral("openssl did not finish signature verification"));
        return false;
    }
    if (verifier.exitStatus() != QProcess::NormalExit || verifier.exitCode() != 0) {
        const QString detail = QString::fromLocal8Bit(verifier.readAllStandardError()).simplified();
        setError(errorMessage, detail.isEmpty()
            ? QStringLiteral("openssl rejected the publication signature")
            : QStringLiteral("openssl rejected the publication signature: %1").arg(detail));
        return false;
    }
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
                      QStringLiteral("signature_envelope"),
                      QStringLiteral("catalogue_file"), QStringLiteral("catalogue_sha256"),
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
    QString signatureEnvelope;
    QString catalogueFile;
    QString catalogueSha256;
    if (!readString(object, QStringLiteral("repository_tag"), repositoryTag, errorMessage)
        || !readString(object, QStringLiteral("channel"), channel, errorMessage)
        || !readString(object, QStringLiteral("abi"), abi, errorMessage)
        || !readString(object, QStringLiteral("generated_at"), generatedAt, errorMessage)
        || !readString(object, QStringLiteral("source_revision"), sourceRevision, errorMessage)
        || !readString(object, QStringLiteral("signature_status"), signatureStatus, errorMessage)
        || !readString(object, QStringLiteral("signature_fingerprint"), signatureFingerprint, errorMessage)
        || !readString(object, QStringLiteral("signature_envelope"), signatureEnvelope, errorMessage)
        || !readString(object, QStringLiteral("catalogue_file"), catalogueFile, errorMessage)
        || !readString(object, QStringLiteral("catalogue_sha256"), catalogueSha256, errorMessage)) {
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
    const QStringList signatureParts = signatureEnvelope.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (signatureEnvelope.startsWith(QLatin1Char('/'))
        || signatureEnvelope.contains(QRegularExpression(QStringLiteral("\\s")))
        || signatureParts.contains(QStringLiteral(".."))
        || !QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$")).match(signatureEnvelope).hasMatch()) {
        setError(errorMessage, QStringLiteral("signature_envelope must be a safe relative path"));
        return false;
    }
    const QStringList catalogueParts = catalogueFile.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (catalogueFile.startsWith(QLatin1Char('/')) || catalogueFile.contains(QRegularExpression(QStringLiteral("\\s")))
        || catalogueParts.contains(QStringLiteral(".."))
        || !QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$")).match(catalogueFile).hasMatch()) {
        setError(errorMessage, QStringLiteral("catalogue_file must be a safe relative path"));
        return false;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{64}$")).match(catalogueSha256).hasMatch()) {
        setError(errorMessage, QStringLiteral("catalogue_sha256 must contain 64 hexadecimal characters"));
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
        signatureEnvelope,
        catalogueFile,
        catalogueSha256.toLower(),
        packages,
    };
    return true;
}

bool UpdatePlanController::verifyCatalogueIntegrity(QString *errorMessage)
{
    m_cataloguePresent = false;
    m_catalogueDigestValid = false;

    const QFileInfo metadataInfo(m_metadataPath);
    const QString cataloguePath = QDir(metadataInfo.absolutePath()).filePath(m_metadata.catalogueFile);
    const QFileInfo catalogueInfo(cataloguePath);
    if (!catalogueInfo.exists() || !catalogueInfo.isFile() || catalogueInfo.isSymLink()) {
        m_catalogueStatus = QStringLiteral("Catalogue file is missing or is not a regular file.");
        setError(errorMessage, m_catalogueStatus);
        return false;
    }
    m_cataloguePresent = true;

    QFile catalogue(catalogueInfo.absoluteFilePath());
    if (!catalogue.open(QIODevice::ReadOnly)) {
        m_catalogueStatus = QStringLiteral("Catalogue file could not be read.");
        setError(errorMessage, m_catalogueStatus);
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!catalogue.atEnd()) {
        const QByteArray chunk = catalogue.read(1024 * 1024);
        if (chunk.isEmpty() && !catalogue.atEnd()) {
            m_catalogueStatus = QStringLiteral("Catalogue file could not be hashed.");
            setError(errorMessage, m_catalogueStatus);
            return false;
        }
        hash.addData(chunk);
    }

    const QString actualDigest = QString::fromLatin1(hash.result().toHex());
    if (actualDigest != m_metadata.catalogueSha256) {
        m_catalogueStatus = QStringLiteral("Catalogue SHA-256 does not match the publication manifest.");
        setError(errorMessage, m_catalogueStatus);
        return false;
    }

    m_catalogueDigestValid = true;
    m_catalogueStatus = QStringLiteral("Catalogue SHA-256 matches the publication manifest.");
    return true;
}

QString UpdatePlanController::defaultMetadataPath()
{
    const QString userConfigPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/northstar/repository-metadata.json");
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
    m_previewReady = false;
    m_planPreview.clear();
}

void UpdatePlanController::setBlockedPlan(const QString &reason)
{
    m_planStatus = QStringLiteral("Update planning is blocked: %1.").arg(reason);
}
