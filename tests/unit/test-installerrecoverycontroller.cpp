#include "installerrecoverycontroller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestInstallerRecoveryController final : public QObject
{
    Q_OBJECT
private slots:
    void reportsIdleState();
    void exportsSanitizedInterruptedDiagnosticsAndPreparesRetry();
    void rejectsUnexpectedDiagnosticFields();
};

static QString recoveryFixture(QTemporaryDir &directory, bool tamperedDiagnostics = false)
{
    const QString path = directory.filePath(QStringLiteral("recovery.sh"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("#!/bin/sh\ncase \"$1\" in\n");
    file.write("--status)\ncat <<'EOF'\n"
               "INSTALLER_STATUS=interrupted\n"
               "TRANSACTION_ID=nstar-install-0123456789abcdef-4202\n"
               "TARGET=md42\n"
               "LAST_PHASE=datasets-created\n"
               "MUTATION_STARTED=yes\n"
               "RECOVERY_ACTION=cleanup-and-restart-required\n"
               "DISK_MUTATION=executor-controlled\nEOF\n;;\n");
    file.write("--diagnostics)\ncat <<'EOF'\n"
               "NORTHSTAR_INSTALLER_DIAGNOSTICS=1\n"
               "TRANSACTION_ID=nstar-install-0123456789abcdef-4202\n"
               "STATUS=interrupted\n"
               "LOCATION=active\n"
               "TARGET=md42\n"
               "TARGET_MEDIASIZE=68719476736\n"
               "TARGET_SECTORSIZE=512\n"
               "LAYOUT=gpt-uefi-zfs\n"
               "POOL_NAME=nstar_0123456789ab\n"
               "SOURCE_MANIFEST_SHA256=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
               "PAYLOAD_SHA256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
               "PROJECT_COMMIT=0123456789abcdef0123456789abcdef01234567\n"
               "LAST_PHASE=datasets-created\n"
               "MUTATION_STARTED=yes\n"
               "RECOVERY_ACTION=cleanup-and-restart-required\n"
               "JOURNAL_EVENT_COUNT=14\n"
               "JOURNAL_LAST_EVENT=execution-interrupted\n"
               "PRIVATE_DATA=excluded\nEOF\n");
    if (tamperedDiagnostics) file.write("printf '%s\\n' 'HOME_DIRECTORY=/home/private'\n");
    file.write(";;\n--prepare-retry)\ncat <<'EOF'\n"
               "INSTALLER_RETRY=READY\n"
               "TRANSACTION_ID=nstar-install-0123456789abcdef-4202\n"
               "TARGET=md42\n"
               "ARCHIVE=/var/db/northstar/installer/archive/nstar-install-0123456789abcdef-4202\n"
               "NEXT_ACTION=stage-new-reviewed-transaction\n"
               "DISK_MUTATION=none\nEOF\n;;\n*) exit 64;;\nesac\n");
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

static QString idleFixture(QTemporaryDir &directory)
{
    const QString path = directory.filePath(QStringLiteral("idle.sh"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("#!/bin/sh\nprintf '%s\\n' 'INSTALLER_STATUS=idle' 'RECOVERY_ACTION=none' 'DISK_MUTATION=none'\n");
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

void TestInstallerRecoveryController::reportsIdleState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    InstallerRecoveryController controller(nullptr, idleFixture(directory), directory.filePath(QStringLiteral("diagnostics")));
    controller.checkStatus();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QVERIFY(!controller.interruptedExecution());
    QVERIFY(controller.transactionId().isEmpty());
}

void TestInstallerRecoveryController::exportsSanitizedInterruptedDiagnosticsAndPreparesRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString diagnostics = directory.filePath(QStringLiteral("diagnostics"));
    InstallerRecoveryController controller(nullptr, recoveryFixture(directory), diagnostics);
    controller.checkStatus();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.state(), QStringLiteral("interrupted"));
    QVERIFY(controller.interruptedExecution());
    QCOMPARE(controller.targetDevice(), QStringLiteral("md42"));
    QCOMPARE(controller.lastPhase(), QStringLiteral("datasets-created"));
    QVERIFY(controller.mutationStarted());

    controller.exportDiagnostics();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QVERIFY(controller.diagnosticsReady());
    QFile report(controller.diagnosticPath());
    QVERIFY(report.open(QIODevice::ReadOnly));
    const QByteArray contents = report.readAll();
    QVERIFY(contents.contains("PRIVATE_DATA=excluded"));
    QVERIFY(contents.contains("LAST_PHASE=datasets-created"));
    QVERIFY(!contents.contains("/home/"));
    QVERIFY(!contents.contains("password"));

    controller.setRetryConfirmationText(QStringLiteral("md41"));
    QVERIFY(!controller.retryConfirmationReady());
    controller.setRetryConfirmationText(QStringLiteral("md42"));
    QVERIFY(controller.retryConfirmationReady());
    QSignalSpy retrySpy(&controller, &InstallerRecoveryController::retryPrepared);
    controller.prepareCleanRetry();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(retrySpy.count(), 1);
    QCOMPARE(controller.state(), QStringLiteral("retry-ready"));
    QCOMPARE(controller.recoveryAction(), QStringLiteral("stage-new-reviewed-transaction"));
}

void TestInstallerRecoveryController::rejectsUnexpectedDiagnosticFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString diagnostics = directory.filePath(QStringLiteral("diagnostics"));
    InstallerRecoveryController controller(nullptr, recoveryFixture(directory, true), diagnostics);
    controller.checkStatus();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QVERIFY(controller.interruptedExecution());
    controller.exportDiagnostics();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QVERIFY(!controller.diagnosticsReady());
    QVERIFY(controller.diagnosticPath().isEmpty());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("privacy"), Qt::CaseInsensitive));
}

QTEST_MAIN(TestInstallerRecoveryController)
#include "test-installerrecoverycontroller.moc"
