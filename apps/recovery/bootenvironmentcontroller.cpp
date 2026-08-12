#include "bootenvironmentcontroller.h"

#include <QDateTime>
#include <QDir>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace {
constexpr qsizetype MaximumReportBytes = 32768;
const QRegularExpression NamePattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$"));
const QRegularExpression ManagedPattern(
    QStringLiteral("^northstar-before-(development|stable)-r[0-9]{1,9}-[0-9a-f]{7,12}$"));
const QRegularExpression SpacePattern(QStringLiteral("^[0-9]+([.][0-9]+)?[KMGTPE]?$"));
const QRegularExpression CreatedPattern(
    QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}$"));

bool parseKeyValues(const QByteArray &output, QMap<QString, QString> *records, QString *error)
{
    if (output.isEmpty() || output.size() > MaximumReportBytes) {
        *error = QStringLiteral("Recovery returned an empty or oversized report.");
        return false;
    }
    for (const QByteArray &line : output.split('\n')) {
        if (line.isEmpty()) continue;
        if (line.size() > 720 || line.contains('\r') || line.contains('\t')) {
            *error = QStringLiteral("Recovery returned an unsafe record.");
            return false;
        }
        const qsizetype separator = line.indexOf('=');
        if (separator < 1) {
            *error = QStringLiteral("Recovery returned a malformed record.");
            return false;
        }
        const QString key = QString::fromLatin1(line.left(separator));
        const QString value = QString::fromUtf8(line.mid(separator + 1));
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

QString defaultDiagnosticDirectory()
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents).filePath(QStringLiteral("Northstar Recovery Diagnostics"));
}
}

BootEnvironmentController::BootEnvironmentController(QObject *parent,
                                                     QString recoveryCommand,
                                                     QString diagnosticDirectory)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_recoveryCommand(std::move(recoveryCommand))
    , m_diagnosticDirectory(std::move(diagnosticDirectory))
{
    if (m_diagnosticDirectory.isEmpty()) m_diagnosticDirectory = defaultDiagnosticDirectory();
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
                handleFinished(code, static_cast<int>(status));
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            failOperation(QStringLiteral("The boot-environment recovery service is unavailable."));
        }
    });
}

bool BootEnvironmentController::busy() const { return m_busy; }
QString BootEnvironmentController::state() const { return m_state; }
QString BootEnvironmentController::statusMessage() const { return m_statusMessage; }
QVariantList BootEnvironmentController::environments() const { return m_environments; }
QString BootEnvironmentController::selectedEnvironment() const { return m_selectedEnvironment; }
QString BootEnvironmentController::confirmationText() const { return m_confirmationText; }
bool BootEnvironmentController::rebootRequired() const { return m_rebootRequired; }
QString BootEnvironmentController::diagnosticPath() const { return m_diagnosticPath; }

bool BootEnvironmentController::activationReady() const
{
    if (m_busy || m_selectedEnvironment.isEmpty()
        || m_confirmationText.trimmed() != m_selectedEnvironment) return false;
    for (const QVariant &value : m_environments) {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("name")).toString() == m_selectedEnvironment) {
            return entry.value(QStringLiteral("activatable")).toBool();
        }
    }
    return false;
}

void BootEnvironmentController::refresh() { start(Operation::Status); }

void BootEnvironmentController::selectEnvironment(const QString &name)
{
    if (m_busy) return;
    QString accepted;
    for (const QVariant &value : m_environments) {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("name")).toString() == name
            && entry.value(QStringLiteral("managed")).toBool()) {
            accepted = name;
            break;
        }
    }
    m_selectedEnvironment = accepted;
    m_confirmationText.clear();
    emit stateChanged();
}

void BootEnvironmentController::setConfirmationText(const QString &text)
{
    m_confirmationText = text.left(96);
    emit stateChanged();
}

void BootEnvironmentController::scheduleActivation()
{
    if (!activationReady()) {
        m_statusMessage = QStringLiteral("Select an available Northstar recovery point and type its exact name.");
        emit stateChanged();
        return;
    }
    start(Operation::Activate);
}

bool BootEnvironmentController::exportDiagnostics()
{
    if (m_busy || m_environments.isEmpty()) return false;
    if (!QDir().mkpath(m_diagnosticDirectory)) {
        m_statusMessage = QStringLiteral("The diagnostic destination could not be created.");
        emit stateChanged();
        return false;
    }
    QString report = QStringLiteral("Northstar boot-environment diagnostics\nSchema: 1\nGenerated UTC: %1\n\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    for (const QVariant &value : m_environments) {
        const QVariantMap entry = value.toMap();
        report += QStringLiteral("name=%1 flags=%2 space=%3 created=%4 active-now=%5 active-next=%6 managed=%7\n")
            .arg(entry.value(QStringLiteral("name")).toString(),
                 entry.value(QStringLiteral("flags")).toString(),
                 entry.value(QStringLiteral("space")).toString(),
                 entry.value(QStringLiteral("created")).toString(),
                 entry.value(QStringLiteral("activeNow")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"),
                 entry.value(QStringLiteral("activeNext")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"),
                 entry.value(QStringLiteral("managed")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"));
    }
    const QString path = QDir(m_diagnosticDirectory).filePath(QStringLiteral("northstar-boot-environments.txt"));
    QSaveFile file(path);
    const QByteArray bytes = report.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_statusMessage = QStringLiteral("The diagnostic report could not be saved.");
        emit stateChanged();
        return false;
    }
    m_diagnosticPath = path;
    m_statusMessage = QStringLiteral("Sanitized boot-environment diagnostics saved to %1.").arg(path);
    emit stateChanged();
    return true;
}

void BootEnvironmentController::start(Operation operation)
{
    if (m_busy) return;
    QString program;
    QStringList arguments;
    if (!m_recoveryCommand.isEmpty()) {
        program = m_recoveryCommand;
        arguments = operation == Operation::Status
            ? QStringList{QStringLiteral("--status")}
            : QStringList{QStringLiteral("--activate"), m_selectedEnvironment,
                          QStringLiteral("--confirm"), m_selectedEnvironment};
    } else if (operation == Operation::Status) {
        program = QStringLiteral("/usr/local/libexec/northstar-boot-environment");
        arguments = {QStringLiteral("--status")};
    } else {
        program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        if (program.isEmpty()) {
            failOperation(QStringLiteral("PolicyKit authentication is unavailable."));
            return;
        }
        arguments = {QStringLiteral("/usr/local/libexec/northstar-boot-environment"),
                     QStringLiteral("--activate"), m_selectedEnvironment,
                     QStringLiteral("--confirm"), m_selectedEnvironment};
    }
    m_operation = operation;
    m_busy = true;
    m_statusMessage = operation == Operation::Status
        ? QStringLiteral("Reading boot environments...")
        : QStringLiteral("Requesting administrator authorization...");
    emit stateChanged();
    m_process->start(program, arguments);
}

void BootEnvironmentController::handleFinished(int exitCode, int exitStatus)
{
    if (!m_busy) return;
    const Operation operation = m_operation;
    m_operation = Operation::None;
    m_busy = false;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString message = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (message.isEmpty()) message = QStringLiteral("Recovery failed with exit code %1.").arg(exitCode);
        failOperation(message.left(280));
        return;
    }
    QString error;
    const QByteArray output = m_process->readAllStandardOutput();
    const bool accepted = operation == Operation::Status
        ? parseStatus(output, &error) : parseActivation(output, &error);
    if (!accepted) {
        failOperation(error);
        return;
    }
    emit stateChanged();
}

bool BootEnvironmentController::parseStatus(const QByteArray &output, QString *errorMessage)
{
    QMap<QString, QString> records;
    if (!parseKeyValues(output, &records, errorMessage)
        || records.value(QStringLiteral("BOOT_ENVIRONMENT_RECOVERY")) != QStringLiteral("1")) return false;
    bool countOk = false;
    const int count = records.value(QStringLiteral("COUNT")).toInt(&countOk);
    if (!countOk || count < 1 || count > 64 || records.size() != count + 2) {
        *errorMessage = QStringLiteral("Boot-environment count failed validation.");
        return false;
    }
    QVariantList entries;
    bool hasCurrent = false;
    for (int index = 0; index < count; ++index) {
        const QString record = records.value(QStringLiteral("ENTRY_%1").arg(index));
        const QStringList fields = record.split(QLatin1Char('|'), Qt::KeepEmptyParts);
        if (fields.size() != 9 || !NamePattern.match(fields.at(0)).hasMatch()
            || !QStringList{QStringLiteral("-"), QStringLiteral("N"), QStringLiteral("R"),
                            QStringLiteral("NR"), QStringLiteral("RN")}.contains(fields.at(1))
            || fields.at(2).isEmpty() || fields.at(2).size() > 256
            || !SpacePattern.match(fields.at(3)).hasMatch()
            || !CreatedPattern.match(fields.at(4)).hasMatch()) {
            *errorMessage = QStringLiteral("A boot-environment record failed validation.");
            return false;
        }
        for (int field : {5, 6, 7, 8}) {
            if (fields.at(field) != QStringLiteral("yes") && fields.at(field) != QStringLiteral("no")) {
                *errorMessage = QStringLiteral("A boot-environment state failed validation.");
                return false;
            }
        }
        const bool managed = fields.at(7) == QStringLiteral("yes");
        if (managed != ManagedPattern.match(fields.at(0)).hasMatch()
            || (fields.at(8) == QStringLiteral("yes") && (!managed || fields.at(6) == QStringLiteral("yes")))) {
            *errorMessage = QStringLiteral("A boot-environment recovery relation is contradictory.");
            return false;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), fields.at(0));
        entry.insert(QStringLiteral("flags"), fields.at(1));
        entry.insert(QStringLiteral("mountpoint"), fields.at(2));
        entry.insert(QStringLiteral("space"), fields.at(3));
        entry.insert(QStringLiteral("created"), fields.at(4));
        entry.insert(QStringLiteral("activeNow"), fields.at(5) == QStringLiteral("yes"));
        entry.insert(QStringLiteral("activeNext"), fields.at(6) == QStringLiteral("yes"));
        entry.insert(QStringLiteral("managed"), managed);
        entry.insert(QStringLiteral("activatable"), fields.at(8) == QStringLiteral("yes"));
        hasCurrent = hasCurrent || entry.value(QStringLiteral("activeNow")).toBool();
        entries.append(entry);
    }
    if (!hasCurrent) {
        *errorMessage = QStringLiteral("No currently active boot environment was reported.");
        return false;
    }
    m_environments = entries;
    m_state = QStringLiteral("ready");
    m_rebootRequired = false;
    m_statusMessage = QStringLiteral("%1 boot environments found. Only verified Northstar recovery points can be selected.").arg(count);
    if (!m_selectedEnvironment.isEmpty()) selectEnvironment(m_selectedEnvironment);
    return true;
}

bool BootEnvironmentController::parseActivation(const QByteArray &output, QString *errorMessage)
{
    QMap<QString, QString> records;
    if (!parseKeyValues(output, &records, errorMessage) || records.size() != 5
        || records.value(QStringLiteral("BOOT_ENVIRONMENT_RECOVERY")) != QStringLiteral("1")
        || !QStringList{QStringLiteral("scheduled"), QStringLiteral("already-selected")}
                .contains(records.value(QStringLiteral("ACTIVATION")))
        || records.value(QStringLiteral("TARGET")) != m_selectedEnvironment
        || records.value(QStringLiteral("REBOOT_REQUIRED")) != QStringLiteral("yes")
        || records.value(QStringLiteral("ACTIVE_NEXT")) != QStringLiteral("yes")) {
        *errorMessage = QStringLiteral("Boot-environment activation could not be verified.");
        return false;
    }
    m_state = QStringLiteral("activation-scheduled");
    m_rebootRequired = true;
    m_confirmationText.clear();
    m_statusMessage = QStringLiteral("%1 will be used at the next reboot. Save your work and restart when ready.")
        .arg(m_selectedEnvironment);
    return true;
}

void BootEnvironmentController::failOperation(const QString &message)
{
    m_busy = false;
    m_operation = Operation::None;
    m_state = QStringLiteral("error");
    m_statusMessage = message;
    emit stateChanged();
}
