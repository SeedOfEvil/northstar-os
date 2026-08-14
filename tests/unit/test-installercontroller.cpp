#include "installercontroller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestInstallerController final : public QObject
{
    Q_OBJECT
private slots:
    void discoversAndConfirmsEligibleTarget();
    void rejectsUnsafeDiscoveryRecords();
    void stagesAndExecutesConfirmedPlan();
};

static QString fixtureCommand(QTemporaryDir &directory, const QByteArray &output)
{
    const QString path = directory.filePath(QStringLiteral("discover.sh"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("#!/bin/sh\nprintf '%s' '");
    QByteArray escaped = output;
    escaped.replace("'", "'\\''");
    file.write(escaped);
    file.write("'\n");
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

void TestInstallerController::discoversAndConfirmsEligibleTarget()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray output =
        "protocol=2\n"
        "da0\t34359738368\t512\tSystem disk\tvirtio\tyes\tno\tContains the running system\n"
        "da1\t68719476736\t4096\tInstall disk\tvirtio\tno\tyes\tAvailable installation destination\n";
    InstallerController controller(nullptr, fixtureCommand(directory, output));
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.disks()->count(), 2);
    QVERIFY(!controller.selectDisk(0));
    QVERIFY(controller.selectDisk(1));
    QCOMPARE(controller.selectedDevice(), QStringLiteral("da1"));
    controller.setConfirmationText(QStringLiteral("da1"));
    QVERIFY(!controller.confirmationReady());
    controller.setEraseAcknowledged(true);
    QVERIFY(controller.confirmationReady());
    QVERIFY(controller.preparePlan());
    QVERIFY(controller.planReady());
    QVERIFY(controller.planSummary().contains(QStringLiteral("/dev/da1")));
    QVERIFY(controller.planSummary().contains(QStringLiteral("no changes"), Qt::CaseInsensitive));
}

void TestInstallerController::rejectsUnsafeDiscoveryRecords()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray output =
        "protocol=2\n"
        "da0;rm\t34359738368\t512\tUnsafe\tunknown\tno\tyes\tAvailable\n";
    InstallerController controller(nullptr, fixtureCommand(directory, output));
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.disks()->count(), 0);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("unsafe")));
}

void TestInstallerController::stagesAndExecutesConfirmedPlan()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray discovery =
        "protocol=2\n"
        "da1\t68719476736\t512\tInstall disk\tvirtio\tno\tyes\tAvailable installation destination\n";
    const QString discoveryCommand = fixtureCommand(directory, discovery);

    const QString manifestPath = directory.filePath(QStringLiteral("source-manifest.conf"));
    QFile manifest(manifestPath);
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QCOMPARE(manifest.write("schema_version=2\nproduct=Northstar\n"), qint64(35));
    manifest.close();

    const QString stagePath = directory.filePath(QStringLiteral("stage.sh"));
    QFile stage(stagePath);
    QVERIFY(stage.open(QIODevice::WriteOnly));
    stage.write("#!/bin/sh\n[ \"$1\" = --stage ] || exit 64\n"
                "grep -Fx target_device=da1 \"$2\" >/dev/null || exit 65\n"
                "grep -Fx target_sectorsize=512 \"$2\" >/dev/null || exit 66\n"
                "printf '%s\\n' INSTALLER_PREFLIGHT=PASS SOURCE_VERIFICATION=PASS TARGET=da1 "
                "TRANSACTION_ID=nstar-install-0123456789abcdef-42 STATE=/protected "
                "RECOVERY_ACTION=resume-or-abandon-required DISK_MUTATION=none\n");
    stage.close();
    stage.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    const QString executePath = directory.filePath(QStringLiteral("execute.sh"));
    QFile execute(executePath);
    QVERIFY(execute.open(QIODevice::WriteOnly));
    execute.write("#!/bin/sh\n"
                  "[ \"$1\" = --execute ] && [ \"$2\" = nstar-install-0123456789abcdef-42 ] "
                  "&& [ \"$3\" = --confirm-device ] && [ \"$4\" = da1 ] || exit 64\n"
                  "printf '%s\\n' INSTALLER_EXECUTION=PASS "
                  "TRANSACTION_ID=nstar-install-0123456789abcdef-42 TARGET=da1 "
                  "ARCHIVE=/protected/archive INSTALLATION_STATUS=completed\n");
    execute.close();
    execute.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    InstallerController controller(nullptr, discoveryCommand, stagePath, executePath, manifestPath);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QVERIFY(controller.selectDisk(0));
    controller.setConfirmationText(QStringLiteral("da1"));
    controller.setEraseAcknowledged(true);
    QVERIFY(controller.preparePlan());
    QVERIFY(controller.beginInstallation());
    QTRY_VERIFY_WITH_TIMEOUT(controller.installationComplete(), 3000);
    QCOMPARE(controller.transactionId(), QStringLiteral("nstar-install-0123456789abcdef-42"));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("completed")));
}

QTEST_MAIN(TestInstallerController)
#include "test-installercontroller.moc"
