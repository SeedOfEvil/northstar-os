#include "packagetrustcontroller.h"

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

} // namespace

class PackageTrustControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidPolicy();
    void rejectsUnresolvedValues();
    void rejectsUnknownAndDuplicateKeys();
    void rejectsUnsafeRepositoryInputs();
    void reportsMissingPolicyAndBlocksPlanning();
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

void PackageTrustControllerTest::reportsValidPolicyButKeepsPlanningBlocked()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("repository-policy.conf"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(file.write(validPolicy()), static_cast<qint64>(validPolicy().size()));
    file.close();

    PackageTrustController controller(path);
    QVERIFY(controller.policyPresent());
    QVERIFY(controller.policyValid());
    QCOMPARE(controller.channel(), QStringLiteral("development"));
    QVERIFY(controller.trustStatus().contains(QStringLiteral("not connected")));
    QVERIFY(controller.repositoryConfigPreview().contains(QStringLiteral("signature_type")));

    controller.planUpdate();
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("signatures")));
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
}

QTEST_MAIN(PackageTrustControllerTest)

#include "test-packagetrustcontroller.moc"
