#include "installercontroller.h"

#include <QProcess>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <utility>

namespace {
constexpr int MaximumDisks = 64;
constexpr qsizetype MaximumReportBytes = 8192;
constexpr qsizetype MaximumManifestBytes = 65536;
const QRegularExpression DevicePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9._-]{0,31}$"));
const QRegularExpression TransactionPattern(QStringLiteral("^nstar-install-[0-9a-f]{16}-[0-9]{1,10}$"));

bool parseRecords(const QByteArray &output, QMap<QString, QString> *records, QString *error)
{
    if (output.isEmpty() || output.size() > MaximumReportBytes) {
        *error = QStringLiteral("Installer service returned an empty or oversized report.");
        return false;
    }
    for (const QByteArray &line : output.split('\n')) {
        if (line.isEmpty()) continue;
        if (line.size() > 640 || line.contains('\r') || line.contains('\t')) {
            *error = QStringLiteral("Installer service returned an unsafe record.");
            return false;
        }
        const qsizetype separator = line.indexOf('=');
        if (separator < 1) {
            *error = QStringLiteral("Installer service returned a malformed record.");
            return false;
        }
        const QString key = QString::fromLatin1(line.left(separator));
        const QString value = QString::fromUtf8(line.mid(separator + 1));
        if (!QRegularExpression(QStringLiteral("^[A-Z][A-Z0-9_]{0,47}$")).match(key).hasMatch()
            || QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")).match(value).hasMatch()
            || records->contains(key)) {
            *error = QStringLiteral("Installer service returned an unsupported record.");
            return false;
        }
        records->insert(key, value);
    }
    return true;
}
}

InstallerController::InstallerController(QObject *parent, QString discoveryCommand,
                                         QString stageCommand, QString executeCommand,
                                         QString manifestPath)
    : QObject(parent), m_disks(this), m_process(new QProcess(this)),
      m_discoveryCommand(std::move(discoveryCommand)), m_stageCommand(std::move(stageCommand)),
      m_executeCommand(std::move(executeCommand)), m_manifestPath(std::move(manifestPath))
{
    if (m_discoveryCommand.isEmpty()) {
        m_discoveryCommand = qEnvironmentVariable("NORTHSTAR_INSTALLER_DISCOVERY_COMMAND");
    }
    if (m_discoveryCommand.isEmpty()) {
        m_discoveryCommand = QStringLiteral("/usr/local/libexec/northstar-installer-disks");
    }
    if (m_manifestPath.isEmpty()) {
        m_manifestPath = qEnvironmentVariable("NORTHSTAR_INSTALLER_MANIFEST_PATH");
    }
    if (m_manifestPath.isEmpty()) {
        m_manifestPath = QStringLiteral("/var/run/northstar-installer/source/source-manifest.conf");
    }
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (!m_busy) return;
        const Operation completed = m_operation;
        m_operation = Operation::None;
        m_busy = false;
        QString error;
        if (status != QProcess::NormalExit || code != 0) {
            error = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
            if (error.isEmpty()) error = QStringLiteral("Installer service failed with exit code %1.").arg(code);
            if (completed == Operation::Discovery) {
                m_disks.clear();
                resetSelection();
                m_statusMessage = error.left(240);
            } else {
                failInstallation(error.left(240));
                return;
            }
        } else if (completed == Operation::Discovery
                   && !parseDiscovery(m_process->readAllStandardOutput(), &error)) {
            m_disks.clear();
            resetSelection();
            m_statusMessage = error;
        } else if (completed == Operation::Discovery) {
            m_statusMessage = m_disks.count() == 0
                ? QStringLiteral("No installable disk devices were discovered.")
                : QStringLiteral("Select a destination. The current system disk cannot be chosen.");
        } else if (completed == Operation::Stage) {
            if (!parseStageResult(m_process->readAllStandardOutput(), &error)) {
                failInstallation(error);
                return;
            }
            m_installationState = QStringLiteral("installing");
            m_statusMessage = QStringLiteral("Installing verified Northstar files. Do not power off or detach either disk...");
            emit stateChanged();
            startProtected(m_executeCommand, QStringLiteral("/usr/local/libexec/northstar-installer-executor"),
                           {QStringLiteral("--execute"), m_transactionId,
                            QStringLiteral("--confirm-device"), selectedDevice()});
            return;
        } else if (completed == Operation::Execute) {
            if (!parseExecutionResult(m_process->readAllStandardOutput(), &error)) {
                failInstallation(error);
                return;
            }
            m_installationState = QStringLiteral("completed");
            m_statusMessage = QStringLiteral("Northstar installation completed. Shut down, detach the installer disk, and boot the destination disk.");
        }
        emit stateChanged();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            m_busy = false;
            if (m_operation == Operation::Discovery) {
                m_disks.clear();
                resetSelection();
                m_statusMessage = QStringLiteral("The read-only disk discovery service is unavailable.");
            } else {
                failInstallation(QStringLiteral("The authenticated installer service is unavailable."));
                return;
            }
            m_operation = Operation::None;
            emit stateChanged();
        }
    });
}

InstallerDiskModel *InstallerController::disks() { return &m_disks; }
bool InstallerController::busy() const { return m_busy; }
QString InstallerController::statusMessage() const { return m_statusMessage; }
int InstallerController::selectedIndex() const { return m_selectedIndex; }
QString InstallerController::selectedDevice() const
{
    const InstallerDisk *disk = m_disks.diskAt(m_selectedIndex);
    return disk == nullptr ? QString() : disk->device;
}
bool InstallerController::confirmationReady() const
{
    const InstallerDisk *disk = m_disks.diskAt(m_selectedIndex);
    return disk != nullptr && disk->eligible && m_eraseAcknowledged
        && m_confirmationText.trimmed() == disk->device;
}
bool InstallerController::planReady() const { return m_planReady; }
QString InstallerController::planSummary() const { return m_planSummary; }
QString InstallerController::installationState() const { return m_installationState; }
bool InstallerController::installationActive() const
{
    return m_installationState == QStringLiteral("staging")
        || m_installationState == QStringLiteral("installing");
}
bool InstallerController::installationComplete() const { return m_installationState == QStringLiteral("completed"); }
QString InstallerController::transactionId() const { return m_transactionId; }

void InstallerController::refresh()
{
    if (m_busy) return;
    m_busy = true;
    m_operation = Operation::Discovery;
    m_statusMessage = QStringLiteral("Inspecting disks without making changes...");
    m_disks.clear();
    resetSelection();
    emit stateChanged();
    m_process->start(m_discoveryCommand, QStringList{});
}

bool InstallerController::selectDisk(int index)
{
    const InstallerDisk *disk = m_disks.diskAt(index);
    if (disk == nullptr || !disk->eligible) return false;
    m_selectedIndex = index;
    m_confirmationText.clear();
    m_eraseAcknowledged = false;
    m_planReady = false;
    m_planSummary.clear();
    emit stateChanged();
    return true;
}

void InstallerController::setConfirmationText(const QString &text)
{
    m_confirmationText = text.left(64);
    m_planReady = false;
    emit stateChanged();
}

void InstallerController::setEraseAcknowledged(bool acknowledged)
{
    m_eraseAcknowledged = acknowledged;
    m_planReady = false;
    emit stateChanged();
}

bool InstallerController::preparePlan()
{
    if (!confirmationReady()) {
        m_statusMessage = QStringLiteral("Type the exact device name and acknowledge permanent erasure first.");
        emit stateChanged();
        return false;
    }
    const InstallerDisk *disk = m_disks.diskAt(m_selectedIndex);
    m_planSummary = QStringLiteral(
        "Review only — no changes have been made.\n\nDestination: /dev/%1\nCapacity: %2\nPlan: GPT + UEFI system partition + ZFS root\nData policy: erase the entire destination disk")
        .arg(disk->device, disk->sizeText);
    m_planReady = true;
    m_installationState = QStringLiteral("review");
    m_statusMessage = QStringLiteral("Review the destination and start the authenticated installation when ready.");
    emit stateChanged();
    return true;
}

bool InstallerController::beginInstallation()
{
    if (m_busy || !m_planReady || !confirmationReady()) {
        m_statusMessage = QStringLiteral("A fully confirmed installation plan is required.");
        emit stateChanged();
        return false;
    }
    QString error;
    if (!createRequest(&error)) {
        failInstallation(error);
        return false;
    }
    m_installationState = QStringLiteral("staging");
    m_statusMessage = QStringLiteral("Authenticating and revalidating signed release media and destination...");
    m_busy = true;
    emit stateChanged();
    startProtected(m_stageCommand, QStringLiteral("/usr/local/libexec/northstar-installer-engine"),
                   {QStringLiteral("--stage"), m_request->fileName()});
    return true;
}

void InstallerController::resetPlan()
{
    if (m_busy || installationActive()) return;
    m_planReady = false;
    m_planSummary.clear();
    m_installationState = QStringLiteral("idle");
    m_transactionId.clear();
    m_request.reset();
    emit stateChanged();
}

bool InstallerController::createRequest(QString *error)
{
    const InstallerDisk *disk = m_disks.diskAt(m_selectedIndex);
    if (disk == nullptr || !disk->eligible || (disk->sectorSize != 512 && disk->sectorSize != 4096)) {
        *error = QStringLiteral("The reviewed destination identity is incomplete.");
        return false;
    }
    QFileInfo manifestInfo(m_manifestPath);
    QFile manifest(m_manifestPath);
    if (!manifestInfo.isFile() || manifestInfo.isSymLink() || manifestInfo.size() <= 0
        || manifestInfo.size() > MaximumManifestBytes || !manifest.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Authenticated release media is unavailable.");
        return false;
    }
    const QByteArray manifestData = manifest.readAll();
    if (manifestData.size() != manifestInfo.size()) {
        *error = QStringLiteral("The release manifest could not be read completely.");
        return false;
    }
    const QByteArray manifestSha = QCryptographicHash::hash(manifestData, QCryptographicHash::Sha256).toHex();
    const QByteArray descriptionSha = QCryptographicHash::hash(disk->description.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QByteArray poolSeed = manifestSha + disk->device.toUtf8() + QByteArray::number(disk->sizeBytes);
    const QByteArray poolSuffix = QCryptographicHash::hash(poolSeed, QCryptographicHash::Sha256).toHex().left(12);

    m_request = std::make_unique<QTemporaryFile>(QDir::tempPath() + QStringLiteral("/northstar-installer-request.XXXXXX"));
    m_request->setAutoRemove(true);
    if (!m_request->open() || !m_request->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        *error = QStringLiteral("Could not create the protected installation request.");
        m_request.reset();
        return false;
    }
    const QByteArray request = QByteArray("protocol=1\noperation=install\ntarget_device=") + disk->device.toUtf8()
        + "\ntarget_mediasize=" + QByteArray::number(disk->sizeBytes)
        + "\ntarget_sectorsize=" + QByteArray::number(disk->sectorSize)
        + "\ntarget_description_sha256=" + descriptionSha
        + "\nlayout=gpt-uefi-zfs\npool_name=nstar_" + poolSuffix
        + "\nsource_manifest_sha256=" + manifestSha
        + "\nconfirmation=erase-target\nplan_status=verified\n";
    if (m_request->write(request) != request.size() || !m_request->flush()) {
        *error = QStringLiteral("Could not publish the protected installation request.");
        m_request.reset();
        return false;
    }
    return true;
}

void InstallerController::startProtected(const QString &command, const QString &fixedProgram,
                                          const QStringList &arguments)
{
    QString program = command;
    QStringList processArguments = arguments;
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        if (program.isEmpty()) {
            failInstallation(QStringLiteral("PolicyKit authentication is unavailable."));
            return;
        }
        processArguments.prepend(fixedProgram);
    }
    m_operation = arguments.first() == QStringLiteral("--stage") ? Operation::Stage : Operation::Execute;
    m_busy = true;
    m_process->start(program, processArguments);
}

bool InstallerController::parseStageResult(const QByteArray &output, QString *error)
{
    QMap<QString, QString> records;
    if (!parseRecords(output, &records, error)) return false;
    const QString transaction = records.value(QStringLiteral("TRANSACTION_ID"));
    if (records.size() != 7 || records.value(QStringLiteral("INSTALLER_PREFLIGHT")) != QStringLiteral("PASS")
        || records.value(QStringLiteral("SOURCE_VERIFICATION")) != QStringLiteral("PASS")
        || records.value(QStringLiteral("TARGET")) != selectedDevice()
        || records.value(QStringLiteral("DISK_MUTATION")) != QStringLiteral("none")
        || records.value(QStringLiteral("RECOVERY_ACTION")) != QStringLiteral("resume-or-abandon-required")
        || !TransactionPattern.match(transaction).hasMatch()) {
        *error = QStringLiteral("Protected staging returned an invalid result.");
        return false;
    }
    m_transactionId = transaction;
    m_request.reset();
    return true;
}

bool InstallerController::parseExecutionResult(const QByteArray &output, QString *error)
{
    QMap<QString, QString> records;
    if (!parseRecords(output, &records, error)) return false;
    if (records.size() != 5 || records.value(QStringLiteral("INSTALLER_EXECUTION")) != QStringLiteral("PASS")
        || records.value(QStringLiteral("INSTALLATION_STATUS")) != QStringLiteral("completed")
        || records.value(QStringLiteral("TRANSACTION_ID")) != m_transactionId
        || records.value(QStringLiteral("TARGET")) != selectedDevice()) {
        *error = QStringLiteral("Protected execution returned an invalid completion result.");
        return false;
    }
    return true;
}

void InstallerController::failInstallation(const QString &message)
{
    m_busy = false;
    m_operation = Operation::None;
    m_installationState = QStringLiteral("failed");
    m_statusMessage = message.left(240);
    m_request.reset();
    emit stateChanged();
}

bool InstallerController::parseDiscovery(const QByteArray &output, QString *error)
{
    const QList<QByteArray> rawLines = output.split('\n');
    if (rawLines.isEmpty() || rawLines.first().trimmed() != "protocol=2") {
        *error = QStringLiteral("Disk discovery returned an unsupported protocol.");
        return false;
    }
    QList<InstallerDisk> result;
    QSet<QString> devices;
    for (qsizetype i = 1; i < rawLines.size(); ++i) {
        if (rawLines.at(i).trimmed().isEmpty()) continue;
        if (result.size() >= MaximumDisks) {
            *error = QStringLiteral("Disk discovery exceeded the bounded device limit.");
            return false;
        }
        const QList<QByteArray> fields = rawLines.at(i).split('\t');
        if (fields.size() != 8) {
            *error = QStringLiteral("Disk discovery returned a malformed record.");
            return false;
        }
        InstallerDisk disk;
        disk.device = QString::fromUtf8(fields.at(0));
        bool sizeOk = false;
        disk.sizeBytes = fields.at(1).toULongLong(&sizeOk);
        bool sectorOk = false;
        disk.sectorSize = fields.at(2).toUInt(&sectorOk);
        disk.description = QString::fromUtf8(fields.at(3)).trimmed().left(100);
        disk.transport = QString::fromUtf8(fields.at(4)).trimmed().left(32);
        disk.systemDisk = fields.at(5) == "yes";
        disk.eligible = fields.at(6) == "yes";
        disk.reason = QString::fromUtf8(fields.at(7)).trimmed().left(160);
        if (!DevicePattern.match(disk.device).hasMatch() || !sizeOk || disk.sizeBytes == 0
            || !sectorOk || (disk.sectorSize != 512 && disk.sectorSize != 4096)
            || devices.contains(disk.device)
            || (fields.at(5) != "yes" && fields.at(5) != "no")
            || (fields.at(6) != "yes" && fields.at(6) != "no")) {
            *error = QStringLiteral("Disk discovery returned an unsafe record.");
            return false;
        }
        if (disk.systemDisk && disk.eligible) {
            *error = QStringLiteral("Disk discovery marked the active system disk eligible.");
            return false;
        }
        const double gib = static_cast<double>(disk.sizeBytes) / (1024.0 * 1024.0 * 1024.0);
        disk.sizeText = QStringLiteral("%1 GiB").arg(gib, 0, 'f', gib >= 100.0 ? 0 : 1);
        devices.insert(disk.device);
        result.append(std::move(disk));
    }
    m_disks.replace(std::move(result));
    return true;
}

void InstallerController::resetSelection()
{
    m_selectedIndex = -1;
    m_confirmationText.clear();
    m_eraseAcknowledged = false;
    m_planReady = false;
    m_planSummary.clear();
    m_installationState = QStringLiteral("idle");
    m_transactionId.clear();
    m_request.reset();
}
