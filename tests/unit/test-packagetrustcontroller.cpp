#include "packagetrustcontroller.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray validPolicy()
{
    return QByteArray("channel=development\n"
                      "repository_tag=northstar-development\n"
                      "repository_name=Northstar Development\n"
                      "repository_url=pkg+https://packages.example.test/northstar\n"
                      "mirror_type=srv\n"
                      "signature_type=fingerprints\n"
                      "fingerprints_path=/usr/local/etc/pkg/fingerprints/northstar-development\n"
                      "trust_mode=required\n");
}

QByteArray policyForStore(const QString &storePath)
{
    QByteArray policy = validPolicy();
    policy.replace("/usr/local/etc/pkg/fingerprints/northstar-development", storePath.toUtf8());
    return policy;
}

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

} // namespace

class PackageTrustControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidPolicy();
    void acceptsAndRejectsFingerprintFiles();
    void rejectsUnresolvedValues();
    void rejectsUnknownAndDuplicateKeys();
    void rejectsUnsafeRepositoryInputs();
    void reportsMissingPolicyAndBlocksPlanning();
    void reportsFingerprintStoreRejection();
    void reportsValidPolicyButKeepsPlanningBlocked();
};

void PackageTrustControllerTest::acceptsValidPolicy()
{
    PackageRepositoryPolicy policy;
    QString errorMessage;

    QVERIFY2(PackageTrustController::parsePolicy(validPolicy(), policy, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(policy.channel, QStringLiteral("development"));
    QCOMPARE(policy.repositoryTag, QStringLiteral("northstar-development"));
    QCOMPARE(policy.repositoryName, QStringLiteral("Northstar Development"));
    QCOMPARE(policy.repositoryUrl, QStringLiteral("pkg+https://packages.example.test/northstar"));
    QCOMPARE(policy.mirrorType, QStringLiteral("srv"));
    QCOMPARE(policy.signatureType, QStringLiteral("fingerprints"));
    QCOMPARE(policy.fingerprintsPath,
             QStringLiteral("/usr/local/etc/pkg/fingerprints/northstar-development"));
    QCOMPARE(policy.trustMode, QStringLiteral("required"));

    const QString config = PackageTrustController::renderPkgRepositoryConfig(policy);
    QVERIFY(config.contains(QStringLiteral("northstar-development: {")));
    QVERIFY(config.contains(QStringLiteral("signature_type: \"fingerprints\"")));
    QVERIFY(config.contains(QStringLiteral("fingerprints: \"/usr/local/etc/pkg/fingerprints/northstar-development\"")));
}

void PackageTrustControllerTest::acceptsAndRejectsFingerprintFiles()
{
    QString fingerprint;
    QString errorMessage;
    QVERIFY(PackageTrustController::parseFingerprintFile(
        QByteArray("function: sha256\nfingerprint: ") + QByteArray(64, 'A') + QByteArray("\n"),
        fingerprint,
        &errorMessage));
    QCOMPARE(fingerprint, QString(64, QLatin1Char('a')));

    QVERIFY(!PackageTrustController::parseFingerprintFile(
        QByteArray("function: md5\nfingerprint: ") + QByteArray(64, 'a') + QByteArray("\n"),
        fingerprint,
        &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("sha256")));

    QVERIFY(!PackageTrustController::parseFingerprintFile(
        QByteArray("function: sha256\nfingerprint: not-a-fingerprint\n"),
        fingerprint,
        &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("64 hexadecimal")));
}

void PackageTrustControllerTest::rejectsUnresolvedValues()
{
    QByteArray policy = validPolicy();
    policy.replace("https://packages.example.test/northstar", "UNSET");

    QString errorMessage;
    PackageRepositoryPolicy parsed;
    QVERIFY(!PackageTrustController::parsePolicy(policy, parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("unresolved")));
}

void PackageTrustControllerTest::rejectsUnknownAndDuplicateKeys()
{
    QByteArray unknown = validPolicy() + QByteArray("unexpected=value\n");
    QByteArray duplicate = validPolicy() + QByteArray("channel=stable\n");
    QString errorMessage;
    PackageRepositoryPolicy parsed;

    QVERIFY(!PackageTrustController::parsePolicy(unknown, parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("not part of the policy contract")));

    QVERIFY(!PackageTrustController::parsePolicy(duplicate, parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("duplicated")));
}

void PackageTrustControllerTest::rejectsUnsafeRepositoryInputs()
{
    QByteArray insecure = validPolicy();
    insecure.replace("pkg+https://packages.example.test/northstar", "https://packages.example.test/northstar");
    QByteArray unsafePath = validPolicy();
    unsafePath.replace("/usr/local/etc/pkg/fingerprints/northstar-development", "/tmp/../unsafe");
    QString errorMessage;
    PackageRepositoryPolicy parsed;

    QVERIFY(!PackageTrustController::parsePolicy(insecure, parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("pkg+https")));

    QVERIFY(!PackageTrustController::parsePolicy(unsafePath, parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("absolute safe path")));
}

void PackageTrustControllerTest::reportsMissingPolicyAndBlocksPlanning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    PackageTrustController controller(directory.filePath(QStringLiteral("missing.conf")));
    QVERIFY(!controller.policyPresent());
    QVERIFY(!controller.policyValid());
    QVERIFY(controller.trustStatus().contains(QStringLiteral("No signed repository policy")));

    controller.planUpdate();
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
}

void PackageTrustControllerTest::reportsFingerprintStoreRejection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("repository-policy.conf"));
    QVERIFY(writeFile(path, policyForStore(directory.filePath(QStringLiteral("fingerprints")))));

    PackageTrustController controller(path);
    QVERIFY(controller.policyValid());
    QVERIFY(!controller.trustStoreValid());
    QVERIFY(controller.trustStatus().contains(QStringLiteral("fingerprint store was rejected")));
}

void PackageTrustControllerTest::reportsValidPolicyButKeepsPlanningBlocked()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storePath = directory.filePath(QStringLiteral("fingerprints"));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("trusted"))));
    QVERIFY(QDir().mkpath(QDir(storePath).filePath(QStringLiteral("revoked"))));
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("trusted")),
                             QStringLiteral("northstar.pub"),
                             QByteArray(64, 'a')));
    QVERIFY(writeFingerprint(QDir(storePath).filePath(QStringLiteral("revoked")),
                             QStringLiteral("old.pub"),
                             QByteArray(64, 'b')));

    const QString path = directory.filePath(QStringLiteral("repository-policy.conf"));
    QVERIFY(writeFile(path, policyForStore(storePath)));

    PackageTrustController controller(path);
    QVERIFY(controller.policyPresent());
    QVERIFY(controller.policyValid());
    QVERIFY(controller.trustStoreValid());
    QCOMPARE(controller.trustedFingerprintCount(), 1);
    QCOMPARE(controller.revokedFingerprintCount(), 1);
    QCOMPARE(controller.channel(), QStringLiteral("development"));
    QVERIFY(controller.trustStatus().contains(QStringLiteral("publication signatures")));
    QVERIFY(controller.repositoryConfigPreview().contains(QStringLiteral("signature_type")));

    controller.planUpdate();
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("verified preview")));
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
}

QTEST_MAIN(PackageTrustControllerTest)

#include "test-packagetrustcontroller.moc"
