#include "packagetrustcontroller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray validPolicy()
{
    return QByteArray("channel=development\n"
                      "repository_name=Northstar Development\n"
                      "repository_url=https://packages.example.test/northstar\n"
                      "signing_key_fingerprint=SHA256:")
        + QByteArray(64, 'a')
        + QByteArray("\ntrust_mode=required\n");
}

} // namespace

class PackageTrustControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidPolicy();
    void rejectsUnresolvedValues();
    void rejectsUnknownAndDuplicateKeys();
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
    QCOMPARE(policy.repositoryName, QStringLiteral("Northstar Development"));
    QCOMPARE(policy.repositoryUrl, QStringLiteral("https://packages.example.test/northstar"));
    QCOMPARE(policy.signingKeyFingerprint, QStringLiteral("SHA256:") + QString(64, QLatin1Char('a')));
    QCOMPARE(policy.trustMode, QStringLiteral("required"));
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

    controller.planUpdate();
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("signatures")));
    QVERIFY(controller.updatePlanStatus().contains(QStringLiteral("blocked"), Qt::CaseInsensitive));
}

QTEST_MAIN(PackageTrustControllerTest)

#include "test-packagetrustcontroller.moc"
