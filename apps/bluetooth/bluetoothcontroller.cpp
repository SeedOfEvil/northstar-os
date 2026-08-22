#include "bluetoothcontroller.h"

#include <algorithm>

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
const QRegularExpression AddressHexPattern(QStringLiteral("^[0-9a-f]{12}$"));
const QRegularExpression NameHexPattern(QStringLiteral("^(?:[0-9a-f]{2}){0,124}$"));

QString colonAddress(const QString &hex)
{
    QStringList octets;
    for (qsizetype i = 0; i < hex.size(); i += 2) octets.append(hex.mid(i, 2));
    return octets.join(QLatin1Char(':'));
}
}

BluetoothController::BluetoothController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_statusMessage(QStringLiteral("Choose Refresh to find discoverable Bluetooth devices."))
{
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy)
            finish(false, QStringLiteral("The Bluetooth scanner could not be started."));
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_busy) return;
        const QByteArray output = m_process->readAllStandardOutput();
        QString error = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            parseScan(output);
            finish(true, m_devices.isEmpty()
                ? QStringLiteral("No discoverable Bluetooth devices were found.")
                : QStringLiteral("Found %1 discoverable Bluetooth device(s).").arg(m_devices.size()));
        } else {
            if (error.startsWith(QStringLiteral("ERROR: "))) error.remove(0, 7);
            finish(false, error.isEmpty() ? QStringLiteral("Bluetooth scanning failed.") : error.left(240));
        }
    });
}

bool BluetoothController::busy() const { return m_busy; }
QString BluetoothController::statusMessage() const { return m_statusMessage; }
bool BluetoothController::statusIsError() const { return m_statusIsError; }
QVariantList BluetoothController::devices() const { return m_devices; }

bool BluetoothController::refreshDevices()
{
    if (m_busy) return false;
    QString program = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_SCAN_COMMAND");
    if (program.isEmpty())
        program = QStringLiteral("/usr/local/libexec/northstar-bluetooth-scan");
    if (!QFileInfo::exists(program)) {
        finish(false, QStringLiteral("The Northstar Bluetooth scanner is not installed."));
        return false;
    }
    m_busy = true;
    m_statusIsError = false;
    m_statusMessage = QStringLiteral("Scanning for discoverable Bluetooth devices...");
    emit stateChanged();
    m_process->setProgram(program);
    m_process->setArguments({});
    m_process->start();
    return true;
}

bool BluetoothController::openSetupWizard(const QString &addressHex)
{
    if (m_busy || !AddressHexPattern.match(addressHex).hasMatch()) {
        finish(false, QStringLiteral("Choose a valid scanned Bluetooth device."));
        return false;
    }

    QString program = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_SETUP_COMMAND");
    QStringList arguments;
    if (!program.isEmpty()) {
        arguments << addressHex;
    } else {
        program = QStandardPaths::findExecutable(QStringLiteral("qterminal"));
        const QString sudo = QStandardPaths::findExecutable(QStringLiteral("sudo"));
        if (program.isEmpty() || sudo.isEmpty()) {
            finish(false, QStringLiteral("The foreground Bluetooth pairing wizard is unavailable."));
            return false;
        }
        arguments << QStringLiteral("-e") << sudo
                  << QStringLiteral("/usr/sbin/bluetooth-config")
                  << QStringLiteral("scan")
                  << QStringLiteral("-d") << colonAddress(addressHex)
                  << QStringLiteral("-n") << QStringLiteral("ubt0hci");
    }

    if (!QProcess::startDetached(program, arguments)) {
        finish(false, QStringLiteral("The Bluetooth pairing wizard could not be opened."));
        return false;
    }
    m_statusIsError = false;
    m_statusMessage = QStringLiteral("Pairing wizard opened in Terminal.");
    emit stateChanged();
    emit setupWizardLaunched();
    return true;
}

void BluetoothController::parseScan(const QByteArray &output)
{
    QVariantList parsed;
    for (const QByteArray &line : output.split('\n')) {
        if (!line.startsWith("device=")) continue;
        const QList<QByteArray> fields = line.mid(7).split('|');
        if (fields.size() != 2) continue;
        const QString addressHex = QString::fromLatin1(fields.at(0));
        const QString nameHex = QString::fromLatin1(fields.at(1));
        if (!AddressHexPattern.match(addressHex).hasMatch()
            || !NameHexPattern.match(nameHex).hasMatch()) continue;
        QString name = QString::fromUtf8(QByteArray::fromHex(fields.at(1)));
        if (name.trimmed().isEmpty()) name = QStringLiteral("Unnamed Bluetooth device");
        parsed.append(QVariantMap{{QStringLiteral("name"), name},
                                  {QStringLiteral("addressHex"), addressHex}});
    }
    std::sort(parsed.begin(), parsed.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
            b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    m_devices = parsed;
    emit devicesChanged();
}

void BluetoothController::finish(bool success, const QString &message)
{
    m_busy = false;
    m_statusIsError = !success;
    m_statusMessage = message;
    emit stateChanged();
}
