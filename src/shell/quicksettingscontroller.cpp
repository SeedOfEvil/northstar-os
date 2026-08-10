#include "quicksettingscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <utility>

namespace {

constexpr int CommandTimeoutMilliseconds = 800;

bool commandSucceeded(const QuickSettingsCommandResult &result)
{
    return result.started && result.exitCode == 0;
}

int parseMixerVolume(const QString &output)
{
    static const QRegularExpression decimalExpression(
        QStringLiteral(R"(vol(?:\.volume)?\s*=\s*([0-9]+(?:\.[0-9]+)?))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression legacyExpression(
        QStringLiteral(R"(vol\s+([0-9]+(?:\.[0-9]+)?))"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = decimalExpression.match(output);
    if (!match.hasMatch()) {
        match = legacyExpression.match(output);
    }
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;
    const double rawVolume = match.captured(1).toDouble(&ok);
    if (!ok) {
        return -1;
    }
    const double percentage = rawVolume <= 1.0 ? rawVolume * 100.0 : rawVolume;
    return qBound(0, qRound(percentage), 100);
}

} // namespace

QuickSettingsController::QuickSettingsController(QObject *parent,
                                                 QString settingsPath,
                                                 CommandProvider commandProvider)
    : QObject(parent)
    , m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
    , m_commandProvider(commandProvider ? std::move(commandProvider) : runCommand)
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    m_doNotDisturb = settings.value(QStringLiteral("notifications/doNotDisturb"), false).toBool();
    refresh();
}

bool QuickSettingsController::wifiAvailable() const { return m_wifiAvailable; }
bool QuickSettingsController::wifiEnabled() const { return m_wifiEnabled; }
QString QuickSettingsController::wifiStatus() const { return m_wifiStatus; }
bool QuickSettingsController::bluetoothAvailable() const { return m_bluetoothAvailable; }
bool QuickSettingsController::bluetoothEnabled() const { return m_bluetoothEnabled; }
QString QuickSettingsController::bluetoothStatus() const { return m_bluetoothStatus; }
bool QuickSettingsController::soundAvailable() const { return m_soundAvailable; }
int QuickSettingsController::volume() const { return m_volume; }
QString QuickSettingsController::soundStatus() const { return m_soundStatus; }
bool QuickSettingsController::displayAvailable() const { return m_displayAvailable; }
int QuickSettingsController::displayBrightness() const { return m_displayBrightness; }
QString QuickSettingsController::displayStatus() const { return m_displayStatus; }
bool QuickSettingsController::nightLightAvailable() const { return m_nightLightAvailable; }
bool QuickSettingsController::nightLightEnabled() const { return m_nightLightEnabled; }
QString QuickSettingsController::nightLightStatus() const { return m_nightLightStatus; }
bool QuickSettingsController::doNotDisturb() const { return m_doNotDisturb; }
QString QuickSettingsController::statusMessage() const { return m_statusMessage; }

void QuickSettingsController::refresh()
{
    refreshWifi();
    refreshBluetooth();
    refreshSound();
    refreshDisplay();
    emit capabilitiesChanged();
}

bool QuickSettingsController::setVolume(int volume)
{
    if (!m_soundAvailable) {
        setStatusMessage(QStringLiteral("Volume is unavailable because FreeBSD reported no mixer device."));
        return false;
    }

    const int requestedVolume = qBound(0, volume, 100);
    const QString mixerValue = QStringLiteral("vol.volume=%1")
                                   .arg(QString::number(requestedVolume / 100.0, 'f', 2));
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {QStringLiteral("-s"), mixerValue});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("FreeBSD mixer rejected the volume change."));
        refreshSound();
        emit capabilitiesChanged();
        return false;
    }

    refreshSound();
    emit capabilitiesChanged();
    if (!m_soundAvailable || qAbs(m_volume - requestedVolume) > 2) {
        setStatusMessage(QStringLiteral("Mixer did not confirm the requested volume."));
        return false;
    }
    setStatusMessage(QStringLiteral("Volume confirmed at %1%.").arg(m_volume));
    return true;
}

void QuickSettingsController::toggleDoNotDisturb()
{
    setDoNotDisturb(!m_doNotDisturb);
}

void QuickSettingsController::setDoNotDisturb(bool enabled)
{
    if (m_doNotDisturb == enabled) {
        return;
    }
    m_doNotDisturb = enabled;
    const QFileInfo settingsInfo(m_settingsPath);
    if (QDir().mkpath(settingsInfo.absolutePath())) {
        QSettings settings(m_settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("notifications/doNotDisturb"), enabled);
        settings.sync();
    }
    setStatusMessage(enabled
        ? QStringLiteral("Do Not Disturb enabled; new notifications stay read in history.")
        : QStringLiteral("Do Not Disturb disabled; new notifications can become unread."));
    emit doNotDisturbChanged();
}

QuickSettingsCommandResult QuickSettingsController::runCommand(
    const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start(QIODevice::ReadOnly);

    QuickSettingsCommandResult result;
    result.started = process.waitForStarted(CommandTimeoutMilliseconds);
    if (!result.started) {
        result.standardError = process.errorString();
        return result;
    }
    if (!process.waitForFinished(CommandTimeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(CommandTimeoutMilliseconds);
        result.standardError = QStringLiteral("Capability probe timed out.");
        return result;
    }
    result.exitCode = process.exitCode();
    result.standardOutput = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    result.standardError = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    return result;
}

QString QuickSettingsController::defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("preferences.ini"));
}

void QuickSettingsController::refreshWifi()
{
    m_wifiAvailable = false;
    m_wifiEnabled = false;
    m_wifiStatus = QStringLiteral("No wireless interface detected");

    const QuickSettingsCommandResult listResult = m_commandProvider(
        QStringLiteral("/sbin/ifconfig"), {QStringLiteral("-l")});
    if (!commandSucceeded(listResult)) {
        m_wifiStatus = QStringLiteral("FreeBSD network status unavailable");
        return;
    }

    const QStringList interfaces = listResult.standardOutput.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &interfaceName : interfaces) {
        if (!interfaceName.startsWith(QStringLiteral("wlan"))) {
            continue;
        }
        m_wifiAvailable = true;
        const QuickSettingsCommandResult interfaceResult = m_commandProvider(
            QStringLiteral("/sbin/ifconfig"), {interfaceName});
        if (!commandSucceeded(interfaceResult)) {
            m_wifiStatus = interfaceName + QStringLiteral(" status unavailable");
            return;
        }
        const QString output = interfaceResult.standardOutput;
        m_wifiEnabled = output.contains(QStringLiteral("status: active"), Qt::CaseInsensitive);
        static const QRegularExpression ssidExpression(QStringLiteral(R"(ssid\s+([^\s]+))"));
        const QRegularExpressionMatch ssidMatch = ssidExpression.match(output);
        m_wifiStatus = m_wifiEnabled
            ? (ssidMatch.hasMatch()
                ? QStringLiteral("Connected to %1").arg(ssidMatch.captured(1))
                : QStringLiteral("Wireless link active"))
            : QStringLiteral("Wireless interface inactive");
        return;
    }
}

void QuickSettingsController::refreshBluetooth()
{
    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/usr/sbin/hccontrol"),
        {QStringLiteral("-n"), QStringLiteral("ubt0hci"), QStringLiteral("read_node_list")});
    m_bluetoothAvailable = commandSucceeded(result);
    m_bluetoothEnabled = m_bluetoothAvailable;
    m_bluetoothStatus = m_bluetoothAvailable
        ? QStringLiteral("Bluetooth controller available")
        : QStringLiteral("No Bluetooth adapter detected");
}

void QuickSettingsController::refreshSound()
{
    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {QStringLiteral("-s"), QStringLiteral("vol")});
    const int parsedVolume = commandSucceeded(result) ? parseMixerVolume(result.standardOutput) : -1;
    m_soundAvailable = parsedVolume >= 0;
    if (!m_soundAvailable) {
        m_volume = 0;
        m_soundStatus = QStringLiteral("No mixer device available");
        return;
    }
    m_volume = parsedVolume;
    m_soundStatus = QStringLiteral("FreeBSD mixer - %1%").arg(parsedVolume);
}

void QuickSettingsController::refreshDisplay()
{
    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/sbin/sysctl"),
        {QStringLiteral("-n"), QStringLiteral("hw.acpi.video.lcd0.brightness")});
    bool ok = false;
    const int brightness = result.standardOutput.toInt(&ok);
    m_displayAvailable = commandSucceeded(result) && ok;
    if (!m_displayAvailable) {
        m_displayBrightness = 0;
        m_displayStatus = QStringLiteral("Brightness control unavailable");
        return;
    }
    m_displayBrightness = qBound(0, brightness, 100);
    m_displayStatus = QStringLiteral("Hardware brightness - %1%").arg(m_displayBrightness);
}

void QuickSettingsController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}
