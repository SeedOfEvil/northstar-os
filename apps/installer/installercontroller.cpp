#include "installercontroller.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <utility>

namespace {
constexpr int MaximumDisks = 64;
const QRegularExpression DevicePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9._-]{0,31}$"));
}

InstallerController::InstallerController(QObject *parent, QString discoveryCommand)
    : QObject(parent), m_disks(this), m_process(new QProcess(this)),
      m_discoveryCommand(std::move(discoveryCommand))
{
    if (m_discoveryCommand.isEmpty()) {
        m_discoveryCommand = qEnvironmentVariable("NORTHSTAR_INSTALLER_DISCOVERY_COMMAND");
    }
    if (m_discoveryCommand.isEmpty()) {
        m_discoveryCommand = QStringLiteral("/usr/local/libexec/northstar-installer-disks");
    }
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (!m_busy) return;
        m_busy = false;
        QString error;
        if (status != QProcess::NormalExit || code != 0) {
            error = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
            if (error.isEmpty()) error = QStringLiteral("Disk discovery failed with exit code %1.").arg(code);
            m_disks.clear();
            resetSelection();
            m_statusMessage = error.left(240);
        } else if (!parseDiscovery(m_process->readAllStandardOutput(), &error)) {
            m_disks.clear();
            resetSelection();
            m_statusMessage = error;
        } else {
            m_statusMessage = m_disks.count() == 0
                ? QStringLiteral("No installable disk devices were discovered.")
                : QStringLiteral("Select a destination. The current system disk cannot be chosen.");
        }
        emit stateChanged();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            m_busy = false;
            m_disks.clear();
            resetSelection();
            m_statusMessage = QStringLiteral("The read-only disk discovery service is unavailable.");
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

void InstallerController::refresh()
{
    if (m_busy) return;
    m_busy = true;
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
    m_statusMessage = QStringLiteral("Installation plan prepared. Execution is not enabled in this slice.");
    emit stateChanged();
    return true;
}

void InstallerController::resetPlan()
{
    m_planReady = false;
    m_planSummary.clear();
    emit stateChanged();
}

bool InstallerController::parseDiscovery(const QByteArray &output, QString *error)
{
    const QList<QByteArray> rawLines = output.split('\n');
    if (rawLines.isEmpty() || rawLines.first().trimmed() != "protocol=1") {
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
        if (fields.size() != 7) {
            *error = QStringLiteral("Disk discovery returned a malformed record.");
            return false;
        }
        InstallerDisk disk;
        disk.device = QString::fromUtf8(fields.at(0));
        bool sizeOk = false;
        disk.sizeBytes = fields.at(1).toULongLong(&sizeOk);
        disk.description = QString::fromUtf8(fields.at(2)).trimmed().left(100);
        disk.transport = QString::fromUtf8(fields.at(3)).trimmed().left(32);
        disk.systemDisk = fields.at(4) == "yes";
        disk.eligible = fields.at(5) == "yes";
        disk.reason = QString::fromUtf8(fields.at(6)).trimmed().left(160);
        if (!DevicePattern.match(disk.device).hasMatch() || !sizeOk || disk.sizeBytes == 0
            || devices.contains(disk.device)
            || (fields.at(4) != "yes" && fields.at(4) != "no")
            || (fields.at(5) != "yes" && fields.at(5) != "no")) {
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
}
