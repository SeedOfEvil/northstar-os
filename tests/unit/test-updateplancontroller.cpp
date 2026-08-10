#include "packagetrustcontroller.h"
#include "updateauthorizationcontroller.h"
#include "updateplancontroller.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool writeFingerprint(const QString &directory, const QString &name, const QByteArray &fingerprint)
{
    return writeFile(QDir(directory).filePath(name),
                     QByteArray("function: sha256\n")
                         + QByteArray("fingerprint: ")
                         + fingerprint
                         + QByteArray("\n"));
}

bool runProcess(const QString &program, const QStringList &arguments, QString *errorMessage = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(2000) || !process.waitForFinished(10000)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("process did not finish");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromLocal8Bit(process.readAllStandardError()).simplified();
        }
        return false;
    }
    return true;
}

QByteArray policyForStore(const QString &storePath)
{
    return QByteArray("channel=development\n")
        + QByteArray("repository_tag=northstar-development\n")
        + QByteArray("repository_name=Northstar Development\n")
        + QByteArray("repository_url=pkg+https://packages.example.test/northstar\n")
        + QByteArray("mirror_type=srv\n")
        + QByteArray("signature_type=fingerprints\n")
        + QByteArray("fingerprints_path=")
        + storePath.toUtf8()
        + QByteArray("\ntrust_mode=required\n");
}

QByteArray validMetadata(const QString &repositoryTag = QStringLiteral("northstar-development"),
                         const QString &signatureStatus = QStringLiteral("unverified"))
{
    QJsonObject firstPackage;
    firstPackage.insert(QStringLiteral("name"), QStringLiteral("northstar-shell"));
    firstPackage.insert(QStringLiteral("version"), QStringLiteral("0.2.0"));
    firstPackage.insert(QStringLiteral("origin"), QStringLiteral("desk/northstar-shell"));
    firstPackage.insert(QStringLiteral("source"), QStringLiteral("ports/northstar"));
    firstPackage.insert(QStringLiteral("project_revision"), QStringLiteral("1234567"));

    QJsonObject secondPackage;
    secondPackage.insert(QStringLiteral("name"), QStringLiteral("northstar-welcome"));
    secondPackage.insert(QStringLiteral("version"), QStringLiteral("1.0.0"));
    secondPackage.insert(QStringLiteral("origin"), QStringLiteral("desk/northstar-welcome"));
    secondPackage.insert(QStringLiteral("source"), QStringLiteral("apps/northstar"));
    secondPackage.insert(QStringLiteral("project_revision"), QStringLiteral("89abcde"));

    QJsonObject metadata;
    metadata.insert(QStringLiteral("schema_version"), 1);
    metadata.insert(QStringLiteral("repository_tag"), repositoryTag);
    metadata.insert(QStringLiteral("channel"), QStringLiteral("development"));
    metadata.insert(QStringLiteral("abi"), QStringLiteral("FreeBSD:15:amd64"));
    metadata.insert(QStringLiteral("revision"), 42);
    metadata.insert(QStringLiteral("generated_at"), QStringLiteral("2026-08-09T12:00:00Z"));
    metadata.insert(QStringLiteral("source_revision"), QStringLiteral("abcdef1"));
    metadata.insert(QStringLiteral("signature_status"), signatureStatus);
    metadata.insert(QStringLiteral("signature_fingerprint"), QString(64, QLatin1Char('a')));
    metadata.insert(QStringLiteral("signature_envelope"), QStringLiteral("signature.json"));
    metadata.insert(QStringLiteral("catalogue_file"), QStringLiteral("data.pkg"));
    metadata.insert(QStringLiteral("catalogue_sha256"),
                    QString::fromLatin1(QCryptographicHash::hash(
                        QByteArray("catalogue-fixture\n"), QCryptographicHash::Sha256).toHex()));
    metadata.insert(QStringLiteral("packages"), QJsonArray{firstPackage, secondPackage});
    return QJsonDocument(metadata).toJson(QJsonDocument::Compact);
}

QVariantMap installedPackage(const QString &name, const QString &version)
{
    return QVariantMap{
        {QStringLiteral("name"), name},
        {QStringLiteral("version"), version},
        {QStringLiteral("comment"), QStringLiteral("test package")},
    };
}

} // namespace

class UpdatePlanControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidMetadata();
    void rejectsMalformedAndUnknownMetadata();
    void rejectsUnresolvedAndDuplicateProvenance();
    void rejectsMissingAndMismatchedCatalogue();
    void blocksWithoutMetadata();
    void previewsCandidatesButKeepsExecutionBlocked();
    void verifiesTrustedRsaSignature();
    void blocksPolicyMismatch();
};

void UpdatePlanControllerTest::acceptsValidMetadata()
{
    RepositoryMetadata metadata;
    QString errorMessage;
    QVERIFY2(UpdatePlanController::parseMetadata(validMetadata(), metadata, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(metadata.schemaVersion, 1);
    QCOMPARE(metadata.repositoryTag, QStringLiteral("northstar-development"));
    QCOMPARE(metadata.channel, QStringLiteral("development"));
    QCOMPARE(metadata.abi, QStringLiteral("FreeBSD:15:amd64"));
    QCOMPARE(metadata.revision, 42);
    QCOMPARE(metadata.signatureStatus, QStringLiteral("unverified"));
    QCOMPARE(metadata.catalogueFile, QStringLiteral("data.pkg"));
    QCOMPARE(metadata.catalogueSha256.size(), 64);
    QCOMPARE(metadata.packages.size(), 2);
    QCOMPARE(metadata.packages.first().origin, QStringLiteral("desk/northstar-shell"));
    QCOMPARE(metadata.packages.first().projectRevision, QStringLiteral("1234567"));
}

void UpdatePlanControllerTest::rejectsMalformedAndUnknownMetadata()
{
    RepositoryMetadata metadata;
    QString errorMessage;

    QByteArray unknown = validMetadata();
    unknown.replace("\"packages\":", "\"unexpected\":true,\"packages\":");
    QVERIFY(!UpdatePlanController::parseMetadata(unknown, metadata, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("not part of the repository metadata contract")));

    QVERIFY(!UpdatePlanController::parseMetadata(QByteArray("not-json"), metadata, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("JSON object")));
}

void UpdatePlanControllerTest::rejectsUnresolvedAndDuplicateProvenance()
{
    RepositoryMetadata metadata;
    QString errorMessage;

    QByteArray unresolved = validMetadata();
    unresolved.replace("\"project_revision\":\"1234567\"", "\"project_revision\":\"RESOLVED_BY_BUILDER\"");
    QVERIFY(!UpdatePlanController::parseMetadata(unresolved, metadata, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("resolved")));

    QByteArray duplicate = validMetadata();
    duplicate.replace("northstar-welcome", "northstar-shell");
    QVERIFY(!UpdatePlanController::parseMetadata(duplicate, metadata, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("duplicated")));
}

void UpdatePlanControllerTest::rejectsMissingAndMismatchedCatalogue()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString metadataPath = directory.filePath(QStringLiteral("repository-metadata.json"));
    QVERIFY(writeFile(metadataPath, validMetadata()));

    UpdatePlanController missing(nullptr, metadataPath);
    QVERIFY(!missing.metadataValid());
    QVERIFY(!missing.cataloguePresent());
    QVERIFY(missing.catalogueStatus().contains(QStringLiteral("missing"), Qt::CaseInsensitive));

    QVERIFY(writeFile(directory.filePath(QStringLiteral("data.pkg")), QByteArray("wrong\n")));
    UpdatePlanController mismatched(nullptr, metadataPath);
    QVERIFY(!mismatched.metadataValid());
    QVERIFY(mismatched.cataloguePresent());
    QVERIFY(!mismatched.catalogueDigestValid());
    QVERIFY(mismatched.catalogueStatus().contains(QStringLiteral("does not match"), Qt::CaseInsensitive));
}

void UpdatePlanControllerTest::blocksWithoutMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    UpdatePlanController controller(nullptr, directory.filePath(QStringLiteral("missing.json")));
    QVERIFY(!controller.metadataPresent());
    QVERIFY(!controller.metadataValid());
    QVERIFY(!controller.preview({installedPackage(QStringLiteral("qterminal"), QStringLiteral("1.0"))}));
    QVERIFY(controller.planStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
}

void UpdatePlanControllerTest::previewsCandidatesButKeepsExecutionBlocked()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storePath = directory.filePath(QStringLiteral("fingerprints"));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("trusted"))));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("revoked"))));
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("trusted")),
                             QStringLiteral("northstar.pub"), QByteArray(64, 'a')));
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("revoked")),
                             QStringLiteral("old.pub"), QByteArray(64, 'b')));

    const QString policyPath = directory.filePath(QStringLiteral("repository-policy.conf"));
    QVERIFY(writeFile(policyPath, policyForStore(storePath)));
    PackageTrustController trustController(policyPath);
    QVERIFY(trustController.policyValid());
    QVERIFY(trustController.trustStoreValid());

    const QString metadataPath = directory.filePath(QStringLiteral("repository-metadata.json"));
    QVERIFY(writeFile(directory.filePath(QStringLiteral("data.pkg")), QByteArray("catalogue-fixture\n")));
    QVERIFY(writeFile(metadataPath, validMetadata()));
    UpdatePlanController controller(&trustController, metadataPath);
    QVERIFY(controller.metadataPresent());
    QVERIFY(controller.metadataValid());
    QVERIFY(controller.cataloguePresent());
    QVERIFY(controller.catalogueDigestValid());

    const QVariantList installed{
        installedPackage(QStringLiteral("northstar-shell"), QStringLiteral("0.1.0")),
        installedPackage(QStringLiteral("qterminal"), QStringLiteral("1.0")),
    };
    QVERIFY(controller.preview(installed));
    QCOMPARE(controller.updateCount(), 1);
    QCOMPARE(controller.installCount(), 1);
    QCOMPARE(controller.unmanagedCount(), 1);
    QVERIFY(controller.planPreview().contains(QStringLiteral("1 update candidate")));
    QVERIFY(controller.planPreview().contains(QStringLiteral("1 new package candidate")));
    QVERIFY(controller.planStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
    QVERIFY(controller.planStatus().contains(QStringLiteral("signature"), Qt::CaseInsensitive));
    QVERIFY(controller.metadataStatus().contains(QStringLiteral("not verified"), Qt::CaseInsensitive));
}

void UpdatePlanControllerTest::verifiesTrustedRsaSignature()
{
    const QString opensslPath = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (opensslPath.isEmpty()) {
        QSKIP("openssl is unavailable on this validation host");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storePath = directory.filePath(QStringLiteral("fingerprints"));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("trusted"))));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("revoked"))));

    const QByteArray catalogueBytes("catalogue-fixture\n");
    QVERIFY(writeFile(directory.filePath(QStringLiteral("data.pkg")), catalogueBytes));
    const QString catalogueDigest = QString::fromLatin1(
        QCryptographicHash::hash(catalogueBytes, QCryptographicHash::Sha256).toHex());
    const QString keyPath = directory.filePath(QStringLiteral("repo.key"));
    const QString publicKeyPath = directory.filePath(QStringLiteral("repo.pub"));
    const QString signaturePath = directory.filePath(QStringLiteral("signature.bin"));
    QString processError;
    QVERIFY2(runProcess(opensslPath, {QStringLiteral("genrsa"), QStringLiteral("-out"), keyPath, QStringLiteral("2048")}, &processError),
             qPrintable(processError));
    QVERIFY2(runProcess(opensslPath, {QStringLiteral("rsa"), QStringLiteral("-in"), keyPath,
                                      QStringLiteral("-pubout"), QStringLiteral("-out"), publicKeyPath}, &processError),
             qPrintable(processError));
    QFile publicKeyFile(publicKeyPath);
    QVERIFY(publicKeyFile.open(QIODevice::ReadOnly));
    const QByteArray publicKey = publicKeyFile.readAll();
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex());
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("trusted")),
                             QStringLiteral("northstar.pub"), fingerprint.toUtf8()));

    QJsonDocument metadataDocument = QJsonDocument::fromJson(validMetadata());
    QJsonObject metadata = metadataDocument.object();
    metadata.insert(QStringLiteral("signature_fingerprint"), fingerprint);
    const QByteArray metadataBytes = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    const QString metadataDigest = QString::fromLatin1(
        QCryptographicHash::hash(metadataBytes, QCryptographicHash::Sha256).toHex());
    QVERIFY(writeFile(directory.filePath(QStringLiteral("repository-metadata.json")), metadataBytes));
    const QString payloadPath = directory.filePath(QStringLiteral("payload"));
    QVERIFY(writeFile(payloadPath, metadataDigest.toUtf8()));
    QVERIFY2(runProcess(opensslPath, {QStringLiteral("dgst"), QStringLiteral("-sha256"),
                                      QStringLiteral("-sign"), keyPath, QStringLiteral("-out"), signaturePath,
                                      payloadPath}, &processError),
             qPrintable(processError));
    QFile signatureFile(signaturePath);
    QVERIFY(signatureFile.open(QIODevice::ReadOnly));
    const QByteArray signature = signatureFile.readAll();

    QJsonObject envelope;
    envelope.insert(QStringLiteral("schema_version"), 2);
    envelope.insert(QStringLiteral("type"), QStringLiteral("rsa"));
    envelope.insert(QStringLiteral("payload_type"), QStringLiteral("repository-metadata-sha256"));
    envelope.insert(QStringLiteral("payload"), metadataDigest);
    envelope.insert(QStringLiteral("public_key_pem"), QString::fromUtf8(publicKey));
    envelope.insert(QStringLiteral("signature_base64"), QString::fromLatin1(signature.toBase64()));
    envelope.insert(QStringLiteral("fingerprint_sha256"), fingerprint);
    QVERIFY(writeFile(directory.filePath(QStringLiteral("signature.json")),
                      QJsonDocument(envelope).toJson(QJsonDocument::Compact)));

    const QString policyPath = directory.filePath(QStringLiteral("repository-policy.conf"));
    QVERIFY(writeFile(policyPath, policyForStore(storePath)));
    PackageTrustController trustController(policyPath);
    QVERIFY(trustController.policyValid());
    QVERIFY(trustController.trustStoreValid());

    UpdatePlanController controller(&trustController,
                                    directory.filePath(QStringLiteral("repository-metadata.json")));
    QVERIFY(controller.metadataValid());
    QVERIFY(controller.catalogueDigestValid());
    QVERIFY(controller.signatureVerified());
    QVERIFY(!controller.previewReady());
    QCOMPARE(controller.signatureStatus(), QStringLiteral("verified"));
    QCOMPARE(controller.signatureFingerprint(), fingerprint);
    QCOMPARE(controller.catalogueSha256(), catalogueDigest);
    QCOMPARE(controller.publicationManifestSha256(), metadataDigest);
    QCOMPARE(controller.channel(), QStringLiteral("development"));
    QCOMPARE(controller.repositoryTag(), QStringLiteral("northstar-development"));
    QCOMPARE(controller.abi(), QStringLiteral("FreeBSD:15:amd64"));
    QCOMPARE(controller.generatedAt(), QStringLiteral("2026-08-09T12:00:00Z"));
    QCOMPARE(controller.packageProvenance().size(), 2);
    QVERIFY(controller.metadataStatus().contains(QStringLiteral("verified"), Qt::CaseInsensitive));
    QVERIFY(controller.preview({installedPackage(QStringLiteral("northstar-shell"), QStringLiteral("0.1.0"))}));
    QVERIFY(controller.previewReady());
    QVERIFY(controller.planStatus().contains(QStringLiteral("authorization"), Qt::CaseInsensitive));

    const QString shellPath = QStandardPaths::findExecutable(QStringLiteral("sh"));
    QVERIFY(!shellPath.isEmpty());
    UpdateAuthorizationController authorization(&trustController,
                                                &controller,
                                                shellPath,
                                                shellPath);
    QVERIFY(authorization.preflightValid());
    QVERIFY(authorization.bectlAvailable());
    QVERIFY(authorization.zfsAvailable());
    const bool protectedServiceInstalled =
        QFileInfo(QStringLiteral("/usr/local/libexec/northstar-update-transaction")).isExecutable()
        && QFileInfo(QStringLiteral("/usr/local/libexec/northstar-update-broker")).isExecutable()
        && !QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty();
    QCOMPARE(authorization.authorizationAvailable(), protectedServiceInstalled);
    QCOMPARE(authorization.bootEnvironmentName(),
             QStringLiteral("northstar-before-development-r42-abcdef1"));
    QVERIFY(authorization.status().contains(
        protectedServiceInstalled ? QStringLiteral("ready")
                                  : QStringLiteral("protected transaction service"),
        Qt::CaseInsensitive));
    QVERIFY(authorization.plan().contains(QStringLiteral("Create boot environment"),
                                           Qt::CaseInsensitive));

    UpdateAuthorizationController missingTools(&trustController,
                                               &controller,
                                               directory.filePath(QStringLiteral("missing-bectl")),
                                               directory.filePath(QStringLiteral("missing-zfs")));
    QVERIFY(!missingTools.preflightValid());
    QVERIFY(missingTools.status().contains(QStringLiteral("must be available"),
                                            Qt::CaseInsensitive));

    QByteArray tamperedMetadata = metadataBytes;
    tamperedMetadata.replace("abcdef1", "abcdef2");
    QVERIFY(writeFile(directory.filePath(QStringLiteral("repository-metadata.json")), tamperedMetadata));
    UpdatePlanController tamperedController(
        &trustController, directory.filePath(QStringLiteral("repository-metadata.json")));
    QVERIFY(tamperedController.metadataValid());
    QVERIFY(tamperedController.catalogueDigestValid());
    QVERIFY(!tamperedController.signatureVerified());
    QVERIFY(tamperedController.metadataStatus().contains(QStringLiteral("manifest digest"),
                                                           Qt::CaseInsensitive));
}

void UpdatePlanControllerTest::blocksPolicyMismatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storePath = directory.filePath(QStringLiteral("fingerprints"));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("trusted"))));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("revoked"))));
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("trusted")),
                             QStringLiteral("northstar.pub"), QByteArray(64, 'a')));
    QVERIFY(writeFile(directory.filePath(QStringLiteral("repository-policy.conf")), policyForStore(storePath)));
    PackageTrustController trustController(directory.filePath(QStringLiteral("repository-policy.conf")));
    QVERIFY(trustController.policyValid());
    QVERIFY(trustController.trustStoreValid());

    const QString metadataPath = directory.filePath(QStringLiteral("repository-metadata.json"));
    QVERIFY(writeFile(directory.filePath(QStringLiteral("data.pkg")), QByteArray("catalogue-fixture\n")));
    QVERIFY(writeFile(metadataPath, validMetadata(QStringLiteral("northstar-stable"))));
    UpdatePlanController controller(&trustController, metadataPath);
    QVERIFY(controller.metadataValid());
    QVERIFY(!controller.preview({}));
    QVERIFY(controller.planStatus().contains(QStringLiteral("do not match")));
}

QTEST_MAIN(UpdatePlanControllerTest)

#include "test-updateplancontroller.moc"
