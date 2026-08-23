#include "quicksettingscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QtMath>

#include <utility>

namespace {

constexpr int CommandTimeoutMilliseconds = 800;
constexpr int MinimumAudibleMixerVolume = 60;

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

int perceptualVolumeForMixer(int mixerVolume)
{
    if (mixerVolume <= 0) {
        return 0;
    }

    // The accepted ALC236 laptop is effectively silent through most of the
    // lower hardware range: raw 63% is barely audible and raw 86% sounds near
    // 40%. Compress that usable 60-100 range into the desktop's 1-100 scale,
    // while keeping a true zero available for silence.
    const double normalized = qBound(0, mixerVolume - MinimumAudibleMixerVolume,
                                     100 - MinimumAudibleMixerVolume)
        / double(100 - MinimumAudibleMixerVolume);
    return qBound(0, qRound(normalized * normalized * 100.0), 100);
}

int mixerVolumeForPerceptual(int perceptualVolume)
{
    if (perceptualVolume <= 0) {
        return 0;
    }

    const double normalized = qBound(0, perceptualVolume, 100) / 100.0;
    const int mixerVolume = MinimumAudibleMixerVolume
        + qRound(qSqrt(normalized) * (100 - MinimumAudibleMixerVolume));
    return qBound(MinimumAudibleMixerVolume, mixerVolume, 100);
}

bool parseMixerMuted(const QString &output)
{
    static const QRegularExpression muteExpression(
        QStringLiteral(R"(vol\.mute\s*=\s*on)"),
        QRegularExpression::CaseInsensitiveOption);
    return muteExpression.match(output).hasMatch();
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
bool QuickSettingsController::wifiWritable() const { return m_wifiWritable; }
bool QuickSettingsController::bluetoothWritable() const { return m_bluetoothWritable; }
bool QuickSettingsController::wifiEnabled() const { return m_wifiEnabled; }
QString QuickSettingsController::wifiStatus() const { return m_wifiStatus; }
bool QuickSettingsController::bluetoothAvailable() const { return m_bluetoothAvailable; }
bool QuickSettingsController::bluetoothEnabled() const { return m_bluetoothEnabled; }
QString QuickSettingsController::bluetoothStatus() const { return m_bluetoothStatus; }
bool QuickSettingsController::soundAvailable() const { return m_soundAvailable; }
int QuickSettingsController::volume() const { return m_volume; }
bool QuickSettingsController::muted() const { return m_muted; }
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

QString QuickSettingsController::radioHelperPath()
{
    // A configured path is authoritative, including when it names nothing: the
    // override has to be able to say "no helper here" as well as "this one".
    // Returning a path that does not exist would advertise a control that
    // cannot act, which is the failure this whole surface is built to avoid.
    const QString configuredPath = qEnvironmentVariable("NORTHSTAR_RADIO_HELPER");
    if (!configuredPath.isEmpty()) {
        return QFileInfo(configuredPath).isExecutable() ? configuredPath : QString();
    }

    const QString userPath = QDir::home().filePath(QStringLiteral(".local/bin/northstar-radio"));
    if (QFileInfo(userPath).isExecutable()) {
        return userPath;
    }

    return QStandardPaths::findExecutable(QStringLiteral("northstar-radio"));
}

bool QuickSettingsController::radioControlAvailable()
{
    return !radioHelperPath().isEmpty();
}

bool QuickSettingsController::setRadioEnabled(const QString &radio, bool enabled)
{
    const QString helper = radioHelperPath();
    if (helper.isEmpty()) {
        setStatusMessage(QStringLiteral("Radio control helper is not installed"));
        return false;
    }

    // Only ever two fixed words. Nothing the user typed reaches the helper.
    const QuickSettingsCommandResult result = m_commandProvider(
        helper, {radio, enabled ? QStringLiteral("on") : QStringLiteral("off")});

    if (!result.started) {
        setStatusMessage(QStringLiteral("Radio control helper could not be run"));
        return false;
    }
    if (result.exitCode != 0) {
        // 69 is the helper reporting that the hardware is absent, which is a
        // different thing from the request being malformed or refused.
        setStatusMessage(result.exitCode == 69
            ? QStringLiteral("No %1 hardware is present").arg(radio)
            : QStringLiteral("%1 change was refused").arg(radio));
        refresh();
        return false;
    }

    refresh();
    return true;
}

bool QuickSettingsController::setWifiEnabled(bool enabled)
{
    if (!m_wifiAvailable) {
        setStatusMessage(QStringLiteral("No wireless interface detected"));
        return false;
    }
    if (!setRadioEnabled(QStringLiteral("wifi"), enabled)) {
        return false;
    }
    setStatusMessage(enabled ? QStringLiteral("Wi-Fi enabled") : QStringLiteral("Wi-Fi disabled"));
    return true;
}

bool QuickSettingsController::setBluetoothEnabled(bool enabled)
{
    if (!m_bluetoothAvailable && enabled) {
        setStatusMessage(QStringLiteral("No Bluetooth adapter detected"));
        return false;
    }
    if (!setRadioEnabled(QStringLiteral("bluetooth"), enabled)) {
        return false;
    }
    setStatusMessage(enabled ? QStringLiteral("Bluetooth enabled")
                             : QStringLiteral("Bluetooth disabled"));
    return true;
}

bool QuickSettingsController::setVolume(int volume)
{
    if (!m_soundAvailable) {
        setStatusMessage(QStringLiteral("Volume is unavailable because FreeBSD reported no mixer device."));
        return false;
    }

    const int requestedVolume = qBound(0, volume, 100);
    const int requestedMixerVolume = mixerVolumeForPerceptual(requestedVolume);
    const QString mixerValue = QStringLiteral("vol.volume=%1")
                                   .arg(QString::number(requestedMixerVolume / 100.0, 'f', 2));
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {mixerValue});
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

bool QuickSettingsController::setMuted(bool muted)
{
    if (!m_soundAvailable) {
        setStatusMessage(QStringLiteral("Mute is unavailable because FreeBSD reported no mixer device."));
        return false;
    }

    const QString mixerValue = muted ? QStringLiteral("vol.mute=on")
                                     : QStringLiteral("vol.mute=off");
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {mixerValue});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("FreeBSD mixer rejected the mute change."));
        refreshSound();
        emit capabilitiesChanged();
        return false;
    }

    refreshSound();
    emit capabilitiesChanged();
    if (!m_soundAvailable || m_muted != muted) {
        setStatusMessage(QStringLiteral("Mixer did not confirm the requested mute state."));
        return false;
    }
    setStatusMessage(muted ? QStringLiteral("Output muted.")
                           : QStringLiteral("Output unmuted."));
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
    m_wifiWritable = false;
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

        // The toggle brings the interface administratively up or down, so that
        // is what "enabled" has to mean. Association is a separate, slower
        // thing owned by wpa_supplicant: reporting enabled only once associated
        // would leave the control looking dead for several seconds after it
        // was switched on, and stuck on for a moment after it was switched off.
        static const QRegularExpression flagsExpression(QStringLiteral(R"(flags=[0-9a-fx]*<([^>]*)>)"));
        const QRegularExpressionMatch flagsMatch = flagsExpression.match(output);
        const QStringList flags = flagsMatch.hasMatch()
            ? flagsMatch.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts)
            : QStringList();
        m_wifiEnabled = flags.contains(QStringLiteral("UP"), Qt::CaseInsensitive);
        m_wifiWritable = !radioHelperPath().isEmpty();

        // FreeBSD reports "associated" for a wireless link and "active" for a
        // wired one; accept either rather than depending on which driver is in
        // use.
        const bool associated =
            output.contains(QStringLiteral("status: associated"), Qt::CaseInsensitive)
            || output.contains(QStringLiteral("status: active"), Qt::CaseInsensitive);

        static const QRegularExpression ssidExpression(QStringLiteral(R"(ssid\s+([^\s]+))"));
        const QRegularExpressionMatch ssidMatch = ssidExpression.match(output);

        if (!m_wifiEnabled) {
            m_wifiStatus = QStringLiteral("Wireless interface off");
        } else if (associated && ssidMatch.hasMatch()) {
            m_wifiStatus = QStringLiteral("Connected to %1").arg(ssidMatch.captured(1));
        } else if (associated) {
            m_wifiStatus = QStringLiteral("Wireless link active");
        } else {
            m_wifiStatus = QStringLiteral("Wireless interface on, not connected");
        }
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
    m_bluetoothWritable = m_bluetoothAvailable && !radioHelperPath().isEmpty();
    m_bluetoothStatus = m_bluetoothAvailable
        ? QStringLiteral("Bluetooth controller available")
        : QStringLiteral("No Bluetooth adapter detected");
}

void QuickSettingsController::refreshSound()
{
    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {QStringLiteral("vol")});
    const int mixerVolume = commandSucceeded(result) ? parseMixerVolume(result.standardOutput) : -1;
    m_soundAvailable = mixerVolume >= 0;
    if (!m_soundAvailable) {
        m_volume = 0;
        m_muted = false;
        m_soundStatus = QStringLiteral("No mixer device available");
        return;
    }
    m_volume = perceptualVolumeForMixer(mixerVolume);
    m_muted = parseMixerMuted(result.standardOutput);
    m_soundStatus = m_muted
        ? QStringLiteral("Muted - %1%").arg(m_volume)
        : QStringLiteral("Output - %1%").arg(m_volume);
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
