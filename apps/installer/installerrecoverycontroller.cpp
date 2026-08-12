#include "installerrecoverycontroller.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace {
constexpr qsizetype MaximumReportBytes = 8192;
const QRegularExpression TransactionPattern(
    QStringLiteral("^nstar-install-[0-9a-f]{16}-[0-9]{1,10}$"));
const QRegularExpression DevicePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9._-]{0,31}$"));
const QRegularExpression PhasePattern(QStringLiteral("^[a-z][a-z-]{0,63}$"));
const QRegularExpression HashPattern(QStringLiteral("^[0-9a-f]{64}$"));
const QRegularExpression CommitPattern(QStringLiteral("^[0-9a-f]{40}$"));
const QRegularExpression CountPattern(QStringLiteral("^[0-9]{1,4}$"));

bool parseRecords(const QByteArray &output, QMap<QString, QString> *records, QString *error)
{
    if (output.isEmpty() || output.size() > MaximumReportBytes) {
        *error = QStringLiteral("Recovery returned an empty or oversized report.");
        return false;
    }
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        if (rawLine.isEmpty()) continue;
        if (rawLine.size() > 640 || rawLine.contains('\r') || rawLine.contains('\t')) {
            *error = QStringLiteral("Recovery returned an unsafe record.");
            return false;
        }
        const qsizetype separator = rawLine.indexOf('=');
        if (separator < 1) {
            *error = QStringLiteral("Recovery returned a malformed record.");
            return false;
        }
        const QString key = QString::fromLatin1(rawLine.left(separator));
        const QString value = QString::fromUtf8(rawLine.mid(separator + 1));
        if (!QRegularExpression(QStringLiteral("^[A-Z][A-Z0-9_]{0,47}$")).match(key).hasMatch()
            || QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")).match(value).hasMatch()
            || records->contains(key)) {
            *error = QStringLiteral("Recovery returned an unsupported record.");
            return false;
        }
        records->insert(key, value);
    }
    return true;
}

bool hasExactKeys(const QMap<QString, QString> &records, const QStringList &keys)
{
    return records.keys() == keys;
}

QString userDiagnosticDirectory()
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents).filePath(QStringLiteral("Northstar Installer Diagnostics"));
}
}

InstallerRecoveryController::InstallerRecoveryController(QObject *parent,
                                                         QString recoveryCommand,
                                                         QString diagnosticDirectory)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_recoveryCommand(std::move(recoveryCommand))
    , m_diagnosticDirectory(std::move(diagnosticDirectory))
{
    if (m_diagnosticDirectory.isEmpty()) {
        m_diagnosticDirectory = qEnvironmentVariable("NORTHSTAR_INSTALLER_DIAGNOSTIC_DIRECTORY");
    }
    if (m_diagnosticDirectory.isEmpty()) m_diagnosticDirectory = userDiagnosticDirectory();

    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
                handleFinished(code, static_cast<int>(status));
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            failOperation(QStringLiteral("The authenticated installer recovery service is unavailable."));
        }
    });
}

bool InstallerRecoveryController::busy() const { return m_busy; }
QString InstallerRecoveryController::state() const { return m_state; }
QString InstallerRecoveryController::statusMessage() const { return m_statusMessage; }
QString InstallerRecoveryController::transactionId() const { return m_transactionId; }
QString InstallerRecoveryController::targetDevice() const { return m_targetDevice; }
QString InstallerRecoveryController::lastPhase() const { return m_lastPhase; }
bool InstallerRecoveryController::mutationStarted() const { return m_mutationStarted; }
QString InstallerRecoveryController::recoveryAction() const { return m_recoveryAction; }
bool InstallerRecoveryController::interruptedExecution() const
{
    return m_state == QStringLiteral("interrupted")
        && m_recoveryAction == QStringLiteral("cleanup-and-restart-required");
}
bool InstallerRecoveryController::retryConfirmationReady() const
{
    return interruptedExecution() && !m_targetDevice.isEmpty()
        && m_retryConfirmationText.trimmed() == m_targetDevice;
}
bool InstallerRecoveryController::diagnosticsReady() const { return m_diagnosticsReady; }
QString InstallerRecoveryController::diagnosticPreview() const { return m_diagnosticPreview; }
QString InstallerRecoveryController::diagnosticPath() const { return m_diagnosticPath; }

void InstallerRecoveryController::checkStatus() { start(Operation::Status); }

void InstallerRecoveryController::setRetryConfirmationText(const QString &text)
{
    m_retryConfirmationText = text.left(64);
    emit stateChanged();
}

void InstallerRecoveryController::exportDiagnostics()
{
    if (!interruptedExecution() || m_transactionId.isEmpty()) {
        m_statusMessage = QStringLiteral("Diagnostics require a verified interrupted execution.");
        emit stateChanged();
        return;
    }
    start(Operation::Diagnostics);
}

void InstallerRecoveryController::prepareCleanRetry()
{
    if (!retryConfirmationReady()) {
        m_statusMessage = QStringLiteral("Type the exact target device before preparing a clean retry.");
        emit stateChanged();
        return;
    }
    start(Operation::PrepareRetry);
}

void InstallerRecoveryController::reset()
{
    if (m_busy) return;
    clearTransaction();
    m_state = QStringLiteral("unchecked");
    m_statusMessage = QStringLiteral("Check for a previous interrupted installation.");
    emit stateChanged();
}

void InstallerRecoveryController::start(Operation operation)
{
    if (m_busy) return;
    QString program;
    QStringList arguments;
    if (!m_recoveryCommand.isEmpty()) {
        program = m_recoveryCommand;
        switch (operation) {
        case Operation::Status: arguments = {QStringLiteral("--status")}; break;
        case Operation::Diagnostics:
            arguments = {QStringLiteral("--diagnostics"), m_transactionId}; break;
        case Operation::PrepareRetry:
            arguments = {QStringLiteral("--prepare-retry"), m_transactionId,
                         QStringLiteral("--confirm-device"), m_targetDevice}; break;
        default: return;
        }
    } else {
        program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        if (program.isEmpty()) {
            failOperation(QStringLiteral("PolicyKit authentication is unavailable."));
            return;
        }
        if (operation == Operation::Status) {
            arguments = {QStringLiteral("/usr/local/libexec/northstar-installer-recovery"),
                         QStringLiteral("--status")};
        } else if (operation == Operation::Diagnostics) {
            arguments = {QStringLiteral("/usr/local/libexec/northstar-installer-recovery"),
                         QStringLiteral("--diagnostics"), m_transactionId};
        } else if (operation == Operation::PrepareRetry) {
            arguments = {QStringLiteral("/usr/local/libexec/northstar-installer-recovery"),
                         QStringLiteral("--prepare-retry"), m_transactionId,
                         QStringLiteral("--confirm-device"), m_targetDevice};
        } else {
            return;
        }
    }
    m_operation = operation;
    m_busy = true;
    m_statusMessage = operation == Operation::Status
        ? QStringLiteral("Checking protected installer state...")
        : operation == Operation::Diagnostics
            ? QStringLiteral("Collecting sanitized installer diagnostics...")
            : QStringLiteral("Verifying the interrupted target before a clean retry...");
    emit stateChanged();
    m_process->start(program, arguments);
}

void InstallerRecoveryController::handleFinished(int exitCode, int exitStatus)
{
    if (!m_busy) return;
    const Operation completedOperation = m_operation;
    m_operation = Operation::None;
    m_busy = false;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString message = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (message.isEmpty()) message = QStringLiteral("Recovery service failed with exit code %1.").arg(exitCode);
        failOperation(message.left(240));
        return;
    }

    QString error;
    bool accepted = false;
    const QByteArray output = m_process->readAllStandardOutput();
    switch (completedOperation) {
    case Operation::Status: accepted = parseStatus(output, &error); break;
    case Operation::Diagnostics: accepted = parseDiagnostics(output, &error); break;
    case Operation::PrepareRetry: accepted = parseRetry(output, &error); break;
    default: error = QStringLiteral("Recovery completed without an active operation."); break;
    }
    if (!accepted) {
        failOperation(error);
        return;
    }
    emit stateChanged();
    if (completedOperation == Operation::PrepareRetry) emit retryPrepared();
}

bool InstallerRecoveryController::parseStatus(const QByteArray &output, QString *errorMessage)
{
    QMap<QString, QString> records;
    if (!parseRecords(output, &records, errorMessage)) return false;
    const QString status = records.value(QStringLiteral("INSTALLER_STATUS"));
    const QString action = records.value(QStringLiteral("RECOVERY_ACTION"));
    const QString mutation = records.value(QStringLiteral("DISK_MUTATION"));
    if (status == QStringLiteral("idle")) {
        if (records.size() != 3 || action != QStringLiteral("none") || mutation != QStringLiteral("none")) {
            *errorMessage = QStringLiteral("Idle recovery status failed validation.");
            return false;
        }
        clearTransaction();
        m_state = status;
        m_statusMessage = QStringLiteral("No unfinished installation was found.");
        return true;
    }
    if (status == QStringLiteral("legacy-blocked")) {
        if (records.size() != 3
            || action != QStringLiteral("manual-legacy-resolution-required")
            || mutation != QStringLiteral("none")) {
            *errorMessage = QStringLiteral("Legacy recovery status failed validation.");
            return false;
        }
        clearTransaction();
        m_state = status;
        m_recoveryAction = action;
        m_statusMessage = QStringLiteral("Legacy installer state requires operator recovery before continuing.");
        return true;
    }
    const QString transaction = records.value(QStringLiteral("TRANSACTION_ID"));
    const QString target = records.value(QStringLiteral("TARGET"));
    if (!TransactionPattern.match(transaction).hasMatch() || !DevicePattern.match(target).hasMatch()
        || !QStringList{QStringLiteral("staged"), QStringLiteral("interrupted"),
                        QStringLiteral("executing"), QStringLiteral("completed")}.contains(status)) {
        *errorMessage = QStringLiteral("Protected recovery status failed validation.");
        return false;
    }
    const QStringList stagedKeys{
        QStringLiteral("DISK_MUTATION"), QStringLiteral("INSTALLER_STATUS"),
        QStringLiteral("RECOVERY_ACTION"), QStringLiteral("TARGET"),
        QStringLiteral("TRANSACTION_ID")};
    const QStringList executionKeys{
        QStringLiteral("DISK_MUTATION"), QStringLiteral("INSTALLER_STATUS"),
        QStringLiteral("LAST_PHASE"), QStringLiteral("MUTATION_STARTED"),
        QStringLiteral("RECOVERY_ACTION"), QStringLiteral("TARGET"),
        QStringLiteral("TRANSACTION_ID")};
    if (!hasExactKeys(records, records.contains(QStringLiteral("LAST_PHASE")) ? executionKeys : stagedKeys)) {
        *errorMessage = QStringLiteral("Protected recovery status returned unexpected fields.");
        return false;
    }
    const bool validStateRelation =
        (status == QStringLiteral("staged")
         && action == QStringLiteral("resume-or-abandon-required")
         && mutation == QStringLiteral("none"))
        || (status == QStringLiteral("interrupted")
            && ((action == QStringLiteral("recover-or-abandon-required")
                 && mutation == QStringLiteral("none"))
                || (action == QStringLiteral("cleanup-and-restart-required")
                    && mutation == QStringLiteral("executor-controlled"))))
        || (status == QStringLiteral("executing")
            && action == QStringLiteral("cleanup-and-restart-required")
            && mutation == QStringLiteral("executor-controlled"))
        || (status == QStringLiteral("completed")
            && action == QStringLiteral("finalize-required")
            && mutation == QStringLiteral("executor-controlled"));
    if (!validStateRelation) {
        *errorMessage = QStringLiteral("Protected recovery state is contradictory.");
        return false;
    }
    if (records.contains(QStringLiteral("LAST_PHASE"))
        && !PhasePattern.match(records.value(QStringLiteral("LAST_PHASE"))).hasMatch()) {
        *errorMessage = QStringLiteral("Recovery phase failed validation.");
        return false;
    }
    const QString mutationStarted = records.value(QStringLiteral("MUTATION_STARTED"));
    if (!mutationStarted.isEmpty() && mutationStarted != QStringLiteral("yes")
        && mutationStarted != QStringLiteral("no")) {
        *errorMessage = QStringLiteral("Recovery mutation state failed validation.");
        return false;
    }
    clearTransaction();
    m_state = status;
    m_transactionId = transaction;
    m_targetDevice = target;
    m_lastPhase = records.value(QStringLiteral("LAST_PHASE"));
    m_mutationStarted = mutationStarted == QStringLiteral("yes");
    m_recoveryAction = action;
    m_statusMessage = interruptedExecution()
        ? QStringLiteral("Installation stopped safely at %1. Export diagnostics, then prepare a clean retry.")
              .arg(m_lastPhase)
        : QStringLiteral("A protected installation transaction requires attention: %1.").arg(action);
    return true;
}

bool InstallerRecoveryController::parseDiagnostics(const QByteArray &output, QString *errorMessage)
{
    QMap<QString, QString> records;
    if (!parseRecords(output, &records, errorMessage)) return false;
    const QStringList required{
        QStringLiteral("JOURNAL_EVENT_COUNT"), QStringLiteral("JOURNAL_LAST_EVENT"),
        QStringLiteral("LAST_PHASE"), QStringLiteral("LAYOUT"), QStringLiteral("LOCATION"),
        QStringLiteral("MUTATION_STARTED"), QStringLiteral("NORTHSTAR_INSTALLER_DIAGNOSTICS"),
        QStringLiteral("PAYLOAD_SHA256"), QStringLiteral("POOL_NAME"), QStringLiteral("PRIVATE_DATA"),
        QStringLiteral("PROJECT_COMMIT"), QStringLiteral("RECOVERY_ACTION"),
        QStringLiteral("SOURCE_MANIFEST_SHA256"), QStringLiteral("STATUS"), QStringLiteral("TARGET"),
        QStringLiteral("TARGET_MEDIASIZE"), QStringLiteral("TARGET_SECTORSIZE"),
        QStringLiteral("TRANSACTION_ID")};
    if (!hasExactKeys(records, required)
        || records.value(QStringLiteral("NORTHSTAR_INSTALLER_DIAGNOSTICS")) != QStringLiteral("1")
        || records.value(QStringLiteral("TRANSACTION_ID")) != m_transactionId
        || records.value(QStringLiteral("TARGET")) != m_targetDevice
        || records.value(QStringLiteral("STATUS")) != QStringLiteral("interrupted")
        || records.value(QStringLiteral("LOCATION")) != QStringLiteral("active")
        || records.value(QStringLiteral("PRIVATE_DATA")) != QStringLiteral("excluded")
        || records.value(QStringLiteral("LAST_PHASE")) != m_lastPhase
        || records.value(QStringLiteral("MUTATION_STARTED"))
            != (m_mutationStarted ? QStringLiteral("yes") : QStringLiteral("no"))
        || records.value(QStringLiteral("RECOVERY_ACTION")) != m_recoveryAction
        || records.value(QStringLiteral("LAYOUT")) != QStringLiteral("gpt-uefi-zfs")
        || !QRegularExpression(QStringLiteral("^nstar_[0-9a-f]{12}$"))
                .match(records.value(QStringLiteral("POOL_NAME"))).hasMatch()
        || !QRegularExpression(QStringLiteral("^[0-9]{8,18}$"))
                .match(records.value(QStringLiteral("TARGET_MEDIASIZE"))).hasMatch()
        || !QRegularExpression(QStringLiteral("^(512|4096)$"))
                .match(records.value(QStringLiteral("TARGET_SECTORSIZE"))).hasMatch()
        || !HashPattern.match(records.value(QStringLiteral("SOURCE_MANIFEST_SHA256"))).hasMatch()
        || !HashPattern.match(records.value(QStringLiteral("PAYLOAD_SHA256"))).hasMatch()
        || !CommitPattern.match(records.value(QStringLiteral("PROJECT_COMMIT"))).hasMatch()
        || !CountPattern.match(records.value(QStringLiteral("JOURNAL_EVENT_COUNT"))).hasMatch()
        || records.value(QStringLiteral("JOURNAL_LAST_EVENT")) != QStringLiteral("execution-interrupted")) {
        *errorMessage = QStringLiteral("Installer diagnostics failed the privacy and integrity contract.");
        return false;
    }

    const QStringList outputOrder{
        QStringLiteral("TRANSACTION_ID"), QStringLiteral("STATUS"), QStringLiteral("TARGET"),
        QStringLiteral("TARGET_MEDIASIZE"), QStringLiteral("TARGET_SECTORSIZE"),
        QStringLiteral("LAYOUT"), QStringLiteral("POOL_NAME"),
        QStringLiteral("SOURCE_MANIFEST_SHA256"), QStringLiteral("PAYLOAD_SHA256"),
        QStringLiteral("PROJECT_COMMIT"), QStringLiteral("LAST_PHASE"),
        QStringLiteral("MUTATION_STARTED"), QStringLiteral("RECOVERY_ACTION"),
        QStringLiteral("JOURNAL_EVENT_COUNT"), QStringLiteral("JOURNAL_LAST_EVENT"),
        QStringLiteral("PRIVATE_DATA")};
    QString report = QStringLiteral("Northstar installer diagnostics\nSchema: 1\nGenerated UTC: %1\n\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    for (const QString &key : outputOrder) report += key + QStringLiteral("=") + records.value(key) + QLatin1Char('\n');

    if (!QDir().mkpath(m_diagnosticDirectory)) {
        *errorMessage = QStringLiteral("The diagnostic destination could not be created.");
        return false;
    }
    const QString path = QDir(m_diagnosticDirectory).filePath(
        QStringLiteral("northstar-installer-%1.txt").arg(m_transactionId));
    QSaveFile file(path);
    const QByteArray reportBytes = report.toUtf8();
    if (!file.open(QIODevice::WriteOnly)
        || file.write(reportBytes) != reportBytes.size() || !file.commit()) {
        *errorMessage = QStringLiteral("The sanitized diagnostic report could not be saved.");
        return false;
    }
    m_diagnosticsReady = true;
    m_diagnosticPath = path;
    m_diagnosticPreview = QStringLiteral("Stopped at %1 after %2 journal events. Private paths, logs, and credentials were excluded.")
        .arg(records.value(QStringLiteral("LAST_PHASE")),
             records.value(QStringLiteral("JOURNAL_EVENT_COUNT")));
    m_statusMessage = QStringLiteral("Sanitized diagnostics saved to %1.").arg(path);
    return true;
}

bool InstallerRecoveryController::parseRetry(const QByteArray &output, QString *errorMessage)
{
    QMap<QString, QString> records;
    if (!parseRecords(output, &records, errorMessage)) return false;
    const QStringList required{
        QStringLiteral("ARCHIVE"), QStringLiteral("DISK_MUTATION"),
        QStringLiteral("INSTALLER_RETRY"), QStringLiteral("NEXT_ACTION"),
        QStringLiteral("TARGET"), QStringLiteral("TRANSACTION_ID")};
    if (!hasExactKeys(records, required)
        || records.value(QStringLiteral("INSTALLER_RETRY")) != QStringLiteral("READY")
        || records.value(QStringLiteral("TRANSACTION_ID")) != m_transactionId
        || records.value(QStringLiteral("TARGET")) != m_targetDevice
        || records.value(QStringLiteral("NEXT_ACTION")) != QStringLiteral("stage-new-reviewed-transaction")
        || records.value(QStringLiteral("DISK_MUTATION")) != QStringLiteral("none")) {
        *errorMessage = QStringLiteral("Clean retry preparation returned an invalid result.");
        return false;
    }
    m_state = QStringLiteral("retry-ready");
    m_recoveryAction = QStringLiteral("stage-new-reviewed-transaction");
    m_retryConfirmationText.clear();
    m_statusMessage = QStringLiteral("The failed attempt is preserved. Return to destinations and create a new reviewed plan.");
    return true;
}

void InstallerRecoveryController::clearTransaction()
{
    m_transactionId.clear();
    m_targetDevice.clear();
    m_lastPhase.clear();
    m_recoveryAction.clear();
    m_retryConfirmationText.clear();
    m_diagnosticPreview.clear();
    m_diagnosticPath.clear();
    m_mutationStarted = false;
    m_diagnosticsReady = false;
}

void InstallerRecoveryController::failOperation(const QString &message)
{
    m_busy = false;
    m_operation = Operation::None;
    m_statusMessage = message;
    emit stateChanged();
}
