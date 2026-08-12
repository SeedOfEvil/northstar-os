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
        "protocol=1\n"
        "da0\t34359738368\tSystem disk\tvirtio\tyes\tno\tContains the running system\n"
        "da1\t68719476736\tInstall disk\tvirtio\tno\tyes\tAvailable installation destination\n";
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
        "protocol=1\n"
        "da0;rm\t34359738368\tUnsafe\tunknown\tno\tyes\tAvailable\n";
    InstallerController controller(nullptr, fixtureCommand(directory, output));
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.disks()->count(), 0);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("unsafe")));
}

QTEST_MAIN(TestInstallerController)
#include "test-installercontroller.moc"
