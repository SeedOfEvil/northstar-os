#include "bootenvironmentcontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class BootEnvironmentControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void inventoryActivationAndDiagnostics();
    void rejectsContradictoryInventory();
};

namespace {
QString writeHelper(QTemporaryDir &directory, const QByteArray &body)
{
    const QString path = directory.filePath(QStringLiteral("fake-recovery"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    file.write("#!/bin/sh\nset -eu\n");
    file.write(body);
    file.close();
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}
}

void BootEnvironmentControllerTest::inventoryActivationAndDiagnostics()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helper = writeHelper(directory, R"SCRIPT(
if [ "$1" = --status ]; then
  printf '%s\n' 'BOOT_ENVIRONMENT_RECOVERY=1' 'COUNT=2' \
    'ENTRY_0=default|NR|/|4.43G|2026-08-05 22:35|yes|yes|no|no' \
    'ENTRY_1=northstar-before-development-r78-017fc81040bb|-|-|1.18G|2026-08-10 19:48|no|no|yes|yes'
elif [ "$1" = --activate ]; then
  printf '%s\n' 'BOOT_ENVIRONMENT_RECOVERY=1' 'ACTIVATION=scheduled' \
    "TARGET=$2" 'REBOOT_REQUIRED=yes' 'ACTIVE_NEXT=yes'
else
  exit 64
fi
)SCRIPT");
    QVERIFY(!helper.isEmpty());

    const QString diagnostics = directory.filePath(QStringLiteral("diagnostics"));
    BootEnvironmentController controller(nullptr, helper, diagnostics);
    controller.refresh();
    QTRY_COMPARE(controller.state(), QStringLiteral("ready"));
    QCOMPARE(controller.environments().size(), 2);
    QVERIFY(!controller.rebootRequired());

    const QString target = QStringLiteral("northstar-before-development-r78-017fc81040bb");
    controller.selectEnvironment(QStringLiteral("default"));
    QVERIFY(controller.selectedEnvironment().isEmpty());
    controller.selectEnvironment(target);
    QCOMPARE(controller.selectedEnvironment(), target);
    controller.setConfirmationText(target);
    QVERIFY(controller.activationReady());
    controller.scheduleActivation();
    QTRY_COMPARE(controller.state(), QStringLiteral("activation-scheduled"));
    QVERIFY(controller.rebootRequired());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("next reboot")));

    QVERIFY(controller.exportDiagnostics());
    QFile report(controller.diagnosticPath());
    QVERIFY(report.open(QIODevice::ReadOnly));
    const QByteArray contents = report.readAll();
    QVERIFY(contents.contains(target.toUtf8()));
    QVERIFY(contents.contains("managed=yes"));
    QVERIFY(!contents.contains("mountpoint="));
}

void BootEnvironmentControllerTest::rejectsContradictoryInventory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helper = writeHelper(directory, R"SCRIPT(
printf '%s\n' 'BOOT_ENVIRONMENT_RECOVERY=1' 'COUNT=1' \
  'ENTRY_0=default|NR|/|4.43G|2026-08-05 22:35|yes|yes|yes|yes'
)SCRIPT");
    QVERIFY(!helper.isEmpty());
    BootEnvironmentController controller(nullptr, helper, directory.path());
    controller.refresh();
    QTRY_COMPARE(controller.state(), QStringLiteral("error"));
    QVERIFY(controller.environments().isEmpty());
}

QTEST_GUILESS_MAIN(BootEnvironmentControllerTest)
#include "test-bootenvironmentcontroller.moc"
