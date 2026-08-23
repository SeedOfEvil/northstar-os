#include "quicksettingscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace {

constexpr int CommandTimeoutMilliseconds = 800;
constexpr int MinimumAudibleMixerVolume = 60;

bool commandSucceeded(const QuickSettingsCommandResult &result)
{
    return result.started && result.exitCode == 0;
}

struct MixerVolumes
{
    int left = -1;
    int right = -1;
};

int mixerPercentage(const QString &value)
{
    bool ok = false;
    const double rawVolume = value.toDouble(&ok);
    if (!ok) {
        return -1;
    }
    const double percentage = rawVolume <= 1.0 ? rawVolume * 100.0 : rawVolume;
    return qBound(0, qRound(percentage), 100);
}

MixerVolumes parseMixerVolumes(const QString &output)
{
    static const QRegularExpression decimalExpression(
        QStringLiteral(R"(vol(?:\.volume)?\s*=\s*([0-9]+(?:\.[0-9]+)?)(?::([0-9]+(?:\.[0-9]+)?))?)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression legacyExpression(
        QStringLiteral(R"(vol\s+([0-9]+(?:\.[0-9]+)?)(?::([0-9]+(?:\.[0-9]+)?))?)"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = decimalExpression.match(output);
    if (!match.hasMatch()) {
        match = legacyExpression.match(output);
    }
    if (!match.hasMatch()) {
        return {};
    }

    const int left = mixerPercentage(match.captured(1));
    const int right = match.captured(2).isEmpty()
        ? left : mixerPercentage(match.captured(2));
    return {left, right};
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

int balanceForChannels(int left, int right)
{
    const int maximum = qMax(left, right);
    if (maximum <= 0 || left == right) {
        return 0;
    }
    return left > right
        ? -qRound((left - right) * 100.0 / left)
        : qRound((right - left) * 100.0 / right);
}

QPair<int, int> channelsForVolumeAndBalance(int volume, int balance)
{
    const int boundedVolume = qBound(0, volume, 100);
    const int boundedBalance = qBound(-100, balance, 100);
    if (boundedBalance < 0) {
        return {boundedVolume,
                qRound(boundedVolume * (100 + boundedBalance) / 100.0)};
    }
    return {qRound(boundedVolume * (100 - boundedBalance) / 100.0),
            boundedVolume};
}

QString stereoMixerValue(int volume, int balance)
{
    const QPair<int, int> channels = channelsForVolumeAndBalance(volume, balance);
    const int left = mixerVolumeForPerceptual(channels.first);
    const int right = mixerVolumeForPerceptual(channels.second);
    return QStringLiteral("vol.volume=%1:%2")
        .arg(QString::number(left / 100.0, 'f', 2),
             QString::number(right / 100.0, 'f', 2));
}

bool parseMixerMuted(const QString &output)
{
    static const QRegularExpression muteExpression(
        QStringLiteral(R"(vol\.mute\s*=\s*on)"),
        QRegularExpression::CaseInsensitiveOption);
    return muteExpression.match(output).hasMatch();
}

QString soundOutputLabel(const QString &description)
{
    if (description.contains(QStringLiteral("Internal Analog"), Qt::CaseInsensitive)) {
        return QStringLiteral("Internal Speakers");
    }
    if (description.contains(QStringLiteral("Headphones"), Qt::CaseInsensitive)) {
        return QStringLiteral("Headphones");
    }
    if (description.contains(QStringLiteral("HDMI"), Qt::CaseInsensitive)
        || description.contains(QStringLiteral("DisplayPort"), Qt::CaseInsensitive)
        || description.contains(QStringLiteral("DP "), Qt::CaseInsensitive)) {
        return QStringLiteral("HDMI / DisplayPort");
    }
    return description.trimmed();
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
int QuickSettingsController::balance() const { return m_balance; }
QVariantList QuickSettingsController::soundOutputs() const { return m_soundOutputs; }
int QuickSettingsController::soundOutput() const { return m_soundOutput; }
bool QuickSettingsController::testSoundAvailable() const { return !testTonePath().isEmpty(); }
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
    const QString mixerValue = stereoMixerValue(requestedVolume, m_balance);
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

bool QuickSettingsController::setBalance(int balance)
{
    if (!m_soundAvailable) {
        setStatusMessage(QStringLiteral("Balance is unavailable because FreeBSD reported no mixer device."));
        return false;
    }

    const int requestedBalance = qBound(-100, balance, 100);
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"),
        {stereoMixerValue(m_volume, requestedBalance)});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("FreeBSD mixer rejected the balance change."));
        refreshSound();
        emit capabilitiesChanged();
        return false;
    }

    refreshSound();
    emit capabilitiesChanged();
    if (!m_soundAvailable || qAbs(m_balance - requestedBalance) > 4) {
        setStatusMessage(QStringLiteral("Mixer did not confirm the requested balance."));
        return false;
    }
    setStatusMessage(m_balance == 0
        ? QStringLiteral("Output centered.")
        : QStringLiteral("Balance confirmed at %1.").arg(m_balance));
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

bool QuickSettingsController::setSoundOutput(int unit)
{
    const bool offered = std::any_of(m_soundOutputs.cbegin(), m_soundOutputs.cend(),
                                     [unit](const QVariant &output) {
        return output.toMap().value(QStringLiteral("unit")).toInt() == unit;
    });
    if (!offered) {
        setStatusMessage(QStringLiteral("That audio output is not available."));
        return false;
    }

    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"),
        {QStringLiteral("-d"), QString::number(unit)});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("FreeBSD mixer rejected the output change."));
        refreshSound();
        emit capabilitiesChanged();
        return false;
    }

    refreshSound();
    emit capabilitiesChanged();
    if (m_soundOutput != unit) {
        setStatusMessage(QStringLiteral("Mixer did not confirm the requested audio output."));
        return false;
    }

    const auto selected = std::find_if(m_soundOutputs.cbegin(), m_soundOutputs.cend(),
                                       [unit](const QVariant &output) {
        return output.toMap().value(QStringLiteral("unit")).toInt() == unit;
    });
    const QString label = selected == m_soundOutputs.cend()
        ? QStringLiteral("audio output")
        : selected->toMap().value(QStringLiteral("label")).toString();
    setStatusMessage(QStringLiteral("Audio output changed to %1.").arg(label));
    return true;
}

bool QuickSettingsController::testSound()
{
    if (!m_soundAvailable) {
        setStatusMessage(QStringLiteral("Test sound is unavailable because FreeBSD reported no mixer device."));
        return false;
    }

    const QString tonePath = testTonePath();
    if (tonePath.isEmpty()) {
        setStatusMessage(QStringLiteral("The Northstar test tone is not installed."));
        return false;
    }

    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/bin/dd"),
        {QStringLiteral("if=%1").arg(tonePath),
         QStringLiteral("of=/dev/dsp"),
         QStringLiteral("bs=192000"),
         QStringLiteral("count=1")});
    if (!commandSucceeded(result)) {
        setStatusMessage(QStringLiteral("The selected output could not play the test sound."));
        return false;
    }
    setStatusMessage(QStringLiteral("Test sound played on the selected output."));
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
    const int finishTimeout = program == QStringLiteral("/bin/dd")
        ? 1800 : CommandTimeoutMilliseconds;
    if (!process.waitForFinished(finishTimeout)) {
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

QString QuickSettingsController::testTonePath()
{
    if (qEnvironmentVariableIsSet("NORTHSTAR_TEST_TONE_PATH")) {
        const QString configured = qEnvironmentVariable("NORTHSTAR_TEST_TONE_PATH");
        return QFileInfo(configured).isReadable() ? configured : QString();
    }

    const QString relativePath = QStringLiteral(
        "northstar/audio/northstar-test-tone-s16le-stereo-48k.raw");
    for (const QString &dataRoot
         : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        const QString candidate = QDir(dataRoot).filePath(relativePath);
        if (QFileInfo(candidate).isReadable()) {
            return candidate;
        }
    }
    return {};
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
    refreshSoundOutputs();
    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {QStringLiteral("vol")});
    const MixerVolumes mixerVolumes = commandSucceeded(result)
        ? parseMixerVolumes(result.standardOutput) : MixerVolumes{};
    m_soundAvailable = mixerVolumes.left >= 0 && mixerVolumes.right >= 0;
    if (!m_soundAvailable) {
        m_volume = 0;
        m_muted = false;
        m_balance = 0;
        m_soundStatus = QStringLiteral("No mixer device available");
        return;
    }
    const int left = perceptualVolumeForMixer(mixerVolumes.left);
    const int right = perceptualVolumeForMixer(mixerVolumes.right);
    m_volume = qMax(left, right);
    m_balance = balanceForChannels(left, right);
    m_muted = parseMixerMuted(result.standardOutput);
    QString activeOutput;
    for (const QVariant &output : std::as_const(m_soundOutputs)) {
        const QVariantMap map = output.toMap();
        if (map.value(QStringLiteral("unit")).toInt() == m_soundOutput) {
            activeOutput = map.value(QStringLiteral("label")).toString();
            break;
        }
    }
    const QString level = m_muted ? QStringLiteral("Muted")
                                  : QStringLiteral("%1%").arg(m_volume);
    m_soundStatus = activeOutput.isEmpty()
        ? level
        : QStringLiteral("%1 - %2").arg(activeOutput, level);
}

void QuickSettingsController::refreshSoundOutputs()
{
    m_soundOutputs.clear();
    m_soundOutput = -1;

    const QuickSettingsCommandResult result = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {QStringLiteral("-a")});
    if (!commandSucceeded(result)) {
        return;
    }

    static const QRegularExpression headerExpression(
        QStringLiteral(R"(^pcm([0-9]+):mixer:\s*<([^>]+)>)"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = result.standardOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = headerExpression.match(line.trimmed());
        if (!match.hasMatch()) {
            continue;
        }

        const int unit = match.captured(1).toInt();
        const QString description = match.captured(2).trimmed();
        const bool current = line.contains(QStringLiteral("(default)"), Qt::CaseInsensitive);
        QVariantMap output;
        output.insert(QStringLiteral("unit"), unit);
        output.insert(QStringLiteral("label"), soundOutputLabel(description));
        output.insert(QStringLiteral("description"), description);
        output.insert(QStringLiteral("current"), current);
        m_soundOutputs.append(output);
        if (current) {
            m_soundOutput = unit;
        }
    }
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
