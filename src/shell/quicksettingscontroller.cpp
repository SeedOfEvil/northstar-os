#include "quicksettingscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
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

bool persistWayfireMode(const QString &path, const QString &output, const QString &mode)
{
    const QFileInfo info(path);
    if (info.isSymLink() || (!QDir().mkpath(info.absolutePath()))) {
        return false;
    }

    QString content;
    if (info.exists()) {
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly)) {
            return false;
        }
        content = QString::fromUtf8(input.readAll());
    }

    const QString header = QStringLiteral("[output:%1]").arg(output);
    const QString assignment = QStringLiteral("mode = %1").arg(mode);
    const QRegularExpression sectionExpression(QStringLiteral(R"(^\s*\[[^]]+\]\s*$)"));
    const QRegularExpression modeExpression(QStringLiteral(R"(^\s*mode\s*=)"),
                                            QRegularExpression::CaseInsensitiveOption);
    QStringList rewritten;
    bool targetSection = false;
    bool sectionFound = false;
    bool modeWritten = false;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (sectionExpression.match(line).hasMatch()) {
            if (targetSection && !modeWritten) {
                rewritten.append(assignment);
                modeWritten = true;
            }
            targetSection = line.trimmed() == header;
            sectionFound = sectionFound || targetSection;
        }
        if (targetSection && modeExpression.match(line).hasMatch()) {
            if (!modeWritten) {
                rewritten.append(assignment);
                modeWritten = true;
            }
            continue;
        }
        rewritten.append(line);
    }
    if (targetSection && !modeWritten) {
        rewritten.append(assignment);
        modeWritten = true;
    }
    if (!sectionFound) {
        if (!rewritten.isEmpty() && !rewritten.constLast().isEmpty()) {
            rewritten.append(QString());
        }
        rewritten.append(header);
        rewritten.append(assignment);
    }

    QSaveFile outputFile(path);
    if (!outputFile.open(QIODevice::WriteOnly)
        || outputFile.write(rewritten.join(QLatin1Char('\n')).toUtf8()) < 0) {
        outputFile.cancelWriting();
        return false;
    }
    return outputFile.commit();
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
    m_displayRevertTimer.setInterval(1000);
    QObject::connect(&m_displayRevertTimer, &QTimer::timeout, this, [this]() {
        if (!m_displayModePending) {
            m_displayRevertTimer.stop();
            return;
        }
        --m_displayModeSecondsRemaining;
        if (m_displayModeSecondsRemaining <= 0) {
            revertDisplayMode();
        } else {
            emit capabilitiesChanged();
        }
    });
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
bool QuickSettingsController::displayWritable() const { return m_displayWritable; }
int QuickSettingsController::displayBrightness() const { return m_displayBrightness; }
QString QuickSettingsController::displayStatus() const { return m_displayStatus; }
QVariantList QuickSettingsController::displayModes() const { return m_displayModes; }
QString QuickSettingsController::currentDisplayMode() const { return m_currentDisplayMode; }
QString QuickSettingsController::previousDisplayMode() const { return m_previousDisplayMode; }
QString QuickSettingsController::displayOutputName() const { return m_displayOutputName; }
bool QuickSettingsController::displayModeWritable() const { return m_displayModeWritable; }
bool QuickSettingsController::displayModePending() const { return m_displayModePending; }
int QuickSettingsController::displayModeSecondsRemaining() const { return m_displayModeSecondsRemaining; }
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
    refreshDisplayModes();
    emit capabilitiesChanged();
}

QString QuickSettingsController::wlrRandrPath()
{
    const QString configured = qEnvironmentVariable("NORTHSTAR_WLR_RANDR");
    if (!configured.isEmpty()) {
        return QFileInfo(configured).isExecutable() ? configured : QString();
    }
    const QString installed = QStringLiteral("/usr/local/bin/wlr-randr");
    return QFileInfo(installed).isExecutable()
        ? installed : QStandardPaths::findExecutable(QStringLiteral("wlr-randr"));
}

QString QuickSettingsController::wayfireConfigPath()
{
    const QString configured = qEnvironmentVariable("NORTHSTAR_WAYFIRE_CONFIG").trimmed();
    return configured.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config/wayfire.ini"))
        : QDir::cleanPath(configured);
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
        setStatusMessage(QStringLiteral("Volume is unavailable because no mixer device was reported."));
        return false;
    }

    const int requestedVolume = qBound(0, volume, 100);
    const QString mixerValue = stereoMixerValue(requestedVolume, m_balance);
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {mixerValue});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("The system mixer rejected the volume change."));
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
        setStatusMessage(QStringLiteral("Balance is unavailable because no mixer device was reported."));
        return false;
    }

    const int requestedBalance = qBound(-100, balance, 100);
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"),
        {stereoMixerValue(m_volume, requestedBalance)});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("The system mixer rejected the balance change."));
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
        setStatusMessage(QStringLiteral("Mute is unavailable because no mixer device was reported."));
        return false;
    }

    const QString mixerValue = muted ? QStringLiteral("vol.mute=on")
                                     : QStringLiteral("vol.mute=off");
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/sbin/mixer"), {mixerValue});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("The system mixer rejected the mute change."));
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
        setStatusMessage(QStringLiteral("The system mixer rejected the output change."));
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
        setStatusMessage(QStringLiteral("Test sound is unavailable because no mixer device was reported."));
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

bool QuickSettingsController::setDisplayBrightness(int brightness)
{
    if (!m_displayWritable) {
        setStatusMessage(QStringLiteral("Brightness cannot be changed on this display."));
        return false;
    }

    // Avoid a zero-brightness setting that could leave a laptop panel looking
    // powered off with no obvious route back to the control.
    const int requestedBrightness = qBound(1, brightness, 100);
    const QuickSettingsCommandResult mutation = m_commandProvider(
        QStringLiteral("/usr/bin/backlight"),
        {QString::number(requestedBrightness)});
    if (!commandSucceeded(mutation)) {
        setStatusMessage(QStringLiteral("The system rejected the brightness change."));
        refreshDisplay();
        emit capabilitiesChanged();
        return false;
    }

    refreshDisplay();
    emit capabilitiesChanged();
    if (!m_displayWritable || qAbs(m_displayBrightness - requestedBrightness) > 1) {
        setStatusMessage(QStringLiteral("The display did not confirm the requested brightness."));
        return false;
    }

    setStatusMessage(QStringLiteral("Brightness confirmed at %1%.").arg(m_displayBrightness));
    return true;
}

bool QuickSettingsController::previewDisplayMode(const QString &mode)
{
    if (!m_displayModeWritable || m_displayOutputName.isEmpty()) {
        setStatusMessage(QStringLiteral("Display mode control is unavailable."));
        return false;
    }
    if (m_displayModePending) {
        setStatusMessage(QStringLiteral("Keep or revert the current display preview first."));
        return false;
    }
    const auto offeredMode = std::find_if(m_displayModes.cbegin(), m_displayModes.cend(),
                                          [&mode](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("value")).toString() == mode;
    });
    if (offeredMode == m_displayModes.cend() || mode == m_currentDisplayMode) {
        return offeredMode != m_displayModes.cend();
    }

    const bool customMode = offeredMode->toMap().value(QStringLiteral("custom")).toBool();
    const QString program = wlrRandrPath();
    const QStringList arguments{QStringLiteral("--output"), m_displayOutputName,
                                customMode ? QStringLiteral("--custom-mode")
                                           : QStringLiteral("--mode"),
                                mode};
    if (!commandSucceeded(m_commandProvider(program,
                                            QStringList{QStringLiteral("--dryrun")} + arguments))) {
        setStatusMessage(QStringLiteral("Wayfire rejected that display mode during validation."));
        return false;
    }
    if (!commandSucceeded(m_commandProvider(program, arguments))) {
        setStatusMessage(QStringLiteral("Wayfire could not apply that display mode."));
        return false;
    }

    const auto previousMode = std::find_if(m_displayModes.cbegin(), m_displayModes.cend(),
                                           [this](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("value")).toString()
            == m_currentDisplayMode;
    });
    m_previousDisplayMode = m_currentDisplayMode;
    m_previousDisplayModeCustom = previousMode != m_displayModes.cend()
        && previousMode->toMap().value(QStringLiteral("custom")).toBool();
    m_currentDisplayMode = mode;
    m_displayModePending = true;
    m_pendingDisplayModeCustom = customMode;
    m_pendingDisplayMode = mode;
    m_displayModeSecondsRemaining = 30;
    m_displayRevertTimer.start();
    setStatusMessage(QStringLiteral("Display preview applied. Keep it within 30 seconds."));
    emit displayModeApplied();
    emit capabilitiesChanged();
    return true;
}

bool QuickSettingsController::keepDisplayMode()
{
    if (!m_displayModePending || m_displayOutputName.isEmpty()) {
        return false;
    }

    QSettings preferences(m_settingsPath, QSettings::IniFormat);
    if (m_pendingDisplayModeCustom) {
        // Wayfire treats its output mode field as an advertised EDID mode and
        // live-reloads the file. Writing a custom lower mode there immediately
        // resets this panel to preferred. Keep custom modes in Northstar's own
        // settings and reapply them through --custom-mode at shell startup.
        preferences.setValue(QStringLiteral("display/customMode"),
                             m_pendingDisplayMode);
        preferences.sync();
        if (preferences.status() != QSettings::NoError) {
            setStatusMessage(QStringLiteral("The custom display mode could not be saved."));
            return false;
        }
    } else {
        QString persistentMode = m_currentDisplayMode;
        persistentMode.remove(QStringLiteral("Hz"));
        if (!persistWayfireMode(wayfireConfigPath(), m_displayOutputName, persistentMode)) {
            setStatusMessage(QStringLiteral("The display mode could not be saved."));
            return false;
        }
        preferences.remove(QStringLiteral("display/customMode"));
        preferences.sync();
    }

    m_displayRevertTimer.stop();
    m_displayModePending = false;
    m_displayModeSecondsRemaining = 0;
    m_pendingDisplayModeCustom = false;
    m_pendingDisplayMode.clear();
    m_previousDisplayMode.clear();
    m_previousDisplayModeCustom = false;
    setStatusMessage(QStringLiteral("Display mode saved for %1.").arg(m_displayOutputName));
    emit capabilitiesChanged();
    return true;
}

bool QuickSettingsController::revertDisplayMode()
{
    if (!m_displayModePending || m_previousDisplayMode.isEmpty()
        || m_displayOutputName.isEmpty()) {
        return false;
    }

    const QString previous = m_previousDisplayMode;
    const QuickSettingsCommandResult result = m_commandProvider(
        wlrRandrPath(),
        {QStringLiteral("--output"), m_displayOutputName,
         m_previousDisplayModeCustom ? QStringLiteral("--custom-mode")
                                     : QStringLiteral("--mode"),
         previous});
    m_displayRevertTimer.stop();
    m_displayModePending = false;
    m_displayModeSecondsRemaining = 0;
    m_pendingDisplayModeCustom = false;
    m_pendingDisplayMode.clear();
    m_previousDisplayMode.clear();
    m_previousDisplayModeCustom = false;
    if (!commandSucceeded(result)) {
        setStatusMessage(QStringLiteral("The previous display mode could not be restored."));
        emit capabilitiesChanged();
        return false;
    }
    m_currentDisplayMode = previous;
    setStatusMessage(QStringLiteral("The previous display mode was restored."));
    emit displayModeApplied();
    emit capabilitiesChanged();
    return true;
}

bool QuickSettingsController::restorePersistedCustomDisplayMode()
{
    if (m_displayModePending || m_displayOutputName.isEmpty()) {
        return false;
    }
    QSettings preferences(m_settingsPath, QSettings::IniFormat);
    const QString mode = preferences.value(QStringLiteral("display/customMode"))
                             .toString().trimmed();
    if (mode.isEmpty() || mode == m_currentDisplayMode) {
        return !mode.isEmpty();
    }
    const auto offeredMode = std::find_if(m_displayModes.cbegin(), m_displayModes.cend(),
                                          [&mode](const QVariant &entry) {
        const QVariantMap details = entry.toMap();
        return details.value(QStringLiteral("custom")).toBool()
            && details.value(QStringLiteral("value")).toString() == mode;
    });
    if (offeredMode == m_displayModes.cend()) {
        preferences.remove(QStringLiteral("display/customMode"));
        preferences.sync();
        return false;
    }
    const QStringList arguments{QStringLiteral("--output"), m_displayOutputName,
                                QStringLiteral("--custom-mode"), mode};
    if (!commandSucceeded(m_commandProvider(
            wlrRandrPath(), QStringList{QStringLiteral("--dryrun")} + arguments))
        || !commandSucceeded(m_commandProvider(wlrRandrPath(), arguments))) {
        setStatusMessage(QStringLiteral("The saved custom display mode could not be restored."));
        return false;
    }
    m_currentDisplayMode = mode;
    setStatusMessage(QStringLiteral("Saved custom display mode restored."));
    emit displayModeApplied();
    emit capabilitiesChanged();
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
        ? 1800
        : QFileInfo(program).fileName() == QStringLiteral("wlr-randr")
            ? 5000 : CommandTimeoutMilliseconds;
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
        m_wifiStatus = QStringLiteral("Network status unavailable");
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
    const QuickSettingsCommandResult backlightResult = m_commandProvider(
        QStringLiteral("/usr/bin/backlight"), {QStringLiteral("-q")});
    bool ok = false;
    int brightness = backlightResult.standardOutput.trimmed().toInt(&ok);
    if (commandSucceeded(backlightResult) && ok) {
        m_displayAvailable = true;
        m_displayWritable = true;
        m_displayBrightness = qBound(0, brightness, 100);
        m_displayStatus = QStringLiteral("Hardware brightness - %1%").arg(m_displayBrightness);
        return;
    }

    const QuickSettingsCommandResult acpiResult = m_commandProvider(
        QStringLiteral("/sbin/sysctl"),
        {QStringLiteral("-n"), QStringLiteral("hw.acpi.video.lcd0.brightness")});
    brightness = acpiResult.standardOutput.trimmed().toInt(&ok);
    m_displayAvailable = commandSucceeded(acpiResult) && ok;
    m_displayWritable = false;
    if (!m_displayAvailable) {
        m_displayBrightness = 0;
        m_displayStatus = QStringLiteral("Brightness control unavailable");
        return;
    }
    m_displayBrightness = qBound(0, brightness, 100);
    m_displayStatus = QStringLiteral("Hardware brightness - %1% (read only)")
                          .arg(m_displayBrightness);
}

void QuickSettingsController::refreshDisplayModes()
{
    m_displayModes.clear();
    m_displayOutputName.clear();
    m_currentDisplayMode.clear();
    m_displayModeWritable = false;

    const QString program = wlrRandrPath();
    if (program.isEmpty()) {
        return;
    }
    const QuickSettingsCommandResult result = m_commandProvider(program, {});
    if (!commandSucceeded(result)) {
        return;
    }

    static const QRegularExpression headExpression(
        QStringLiteral(R"(^([^\s]+)\s+\"([^\"]*)\"$)"));
    static const QRegularExpression modeExpression(
        QStringLiteral(R"(^\s+([0-9]+)x([0-9]+) px(?:,\s*([0-9.]+) Hz)?(?:\s+\(([^)]*)\))?$)"));
    QString activeHead;
    bool enabled = false;
    QVariantList activeModes;
    QString activeCurrent;

    const auto commitHead = [&]() {
        if (m_displayOutputName.isEmpty() && enabled && !activeHead.isEmpty()
            && !activeModes.isEmpty()) {
            m_displayOutputName = activeHead;
            m_displayModes = activeModes;
            m_currentDisplayMode = activeCurrent;
        }
    };

    const QStringList lines = result.standardOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch headMatch = headExpression.match(line);
        if (headMatch.hasMatch()) {
            commitHead();
            activeHead = headMatch.captured(1);
            enabled = false;
            activeModes.clear();
            activeCurrent.clear();
            continue;
        }
        if (activeHead.isEmpty()) {
            continue;
        }
        if (line.trimmed() == QStringLiteral("Enabled: yes")) {
            enabled = true;
            continue;
        }
        const QRegularExpressionMatch modeMatch = modeExpression.match(line);
        if (!modeMatch.hasMatch()) {
            continue;
        }
        const QString width = modeMatch.captured(1);
        const QString height = modeMatch.captured(2);
        const QString refresh = modeMatch.captured(3);
        const QString flags = modeMatch.captured(4);
        const QString value = refresh.isEmpty()
            ? QStringLiteral("%1x%2").arg(width, height)
            : QStringLiteral("%1x%2@%3Hz").arg(width, height, refresh);
        bool refreshOk = false;
        const double refreshRate = refresh.toDouble(&refreshOk);
        const QString label = refreshOk
            ? QStringLiteral("%1 × %2 at %3 Hz")
                  .arg(width, height, QString::number(refreshRate, 'f',
                      qFuzzyCompare(refreshRate, qRound(refreshRate)) ? 0 : 3))
            : QStringLiteral("%1 × %2").arg(width, height);
        const bool current = flags.contains(QStringLiteral("current"));
        activeModes.append(QVariantMap{
            {QStringLiteral("value"), value},
            {QStringLiteral("label"), label},
            {QStringLiteral("preferred"), flags.contains(QStringLiteral("preferred"))},
            {QStringLiteral("current"), current},
        });
        if (current) {
            activeCurrent = value;
        }
    }
    commitHead();

    // Internal laptop panels often advertise only native timings even when
    // wlroots can validate lower modes. Offer a short 16:9 set below the
    // largest eDP mode; selection still passes through --dryrun before apply.
    if (m_displayOutputName.startsWith(QStringLiteral("eDP-"))) {
        int maximumWidth = 0;
        int maximumHeight = 0;
        static const QRegularExpression dimensionsExpression(
            QStringLiteral(R"(^([0-9]+)x([0-9]+))"));
        for (const QVariant &modeValue : std::as_const(m_displayModes)) {
            const QRegularExpressionMatch dimensions = dimensionsExpression.match(
                modeValue.toMap().value(QStringLiteral("value")).toString());
            if (dimensions.hasMatch()) {
                maximumWidth = qMax(maximumWidth, dimensions.captured(1).toInt());
                maximumHeight = qMax(maximumHeight, dimensions.captured(2).toInt());
            }
        }

        const QList<QPair<int, int>> lowerModes{{1600, 900}, {1366, 768}, {1280, 720}};
        for (const auto &[width, height] : lowerModes) {
            if (width >= maximumWidth || height >= maximumHeight) {
                continue;
            }
            const QString value = QStringLiteral("%1x%2@60Hz").arg(width).arg(height);
            const auto existing = std::find_if(m_displayModes.cbegin(), m_displayModes.cend(),
                                               [&value](const QVariant &entry) {
                return entry.toMap().value(QStringLiteral("value")).toString() == value;
            });
            if (existing != m_displayModes.cend()) {
                QVariantMap existingMode = existing->toMap();
                existingMode.insert(QStringLiteral("custom"), true);
                m_displayModes[std::distance(m_displayModes.cbegin(), existing)] = existingMode;
                continue;
            }
            m_displayModes.append(QVariantMap{
                {QStringLiteral("value"), value},
                {QStringLiteral("label"), QStringLiteral("%1 × %2 at 60 Hz")
                                                  .arg(width).arg(height)},
                {QStringLiteral("preferred"), false},
                {QStringLiteral("current"), false},
                {QStringLiteral("custom"), true},
            });
        }
    }
    m_displayModeWritable = !m_displayOutputName.isEmpty()
        && !m_currentDisplayMode.isEmpty() && m_displayModes.size() > 1;
}

void QuickSettingsController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}
