#include "bluetoothcontroller.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {
const QRegularExpression AddressHexPattern(QStringLiteral("^[0-9a-f]{12}$"));
const QRegularExpression NameHexPattern(QStringLiteral("^(?:[0-9a-f]{2}){1,124}$"));
const QRegularExpression ConfirmationPattern(QStringLiteral("^[0-9]{6}$"));
}

BluetoothController::BluetoothController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_statusMessage(QStringLiteral("Choose Refresh to find discoverable or remembered Bluetooth devices."))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_standardOutput.append(m_process->readAllStandardOutput());
        if (m_operation != Operation::Scan) processOutput();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            finish(false, m_operation == Operation::Scan
                ? QStringLiteral("The Bluetooth scanner could not be started.")
                : QStringLiteral("The protected Bluetooth service could not be started."));
        }
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_busy) return;
        const Operation completed = m_operation;
        m_standardOutput.append(m_process->readAllStandardOutput());
        QByteArray output;
        if (completed == Operation::Scan) output = m_standardOutput;
        else processOutput(true);
        m_standardOutput.clear();
        QString error = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            if (completed == Operation::Scan) {
                parseScan(output);
                const auto connected = std::find_if(m_devices.cbegin(), m_devices.cend(),
                    [](const QVariant &device) {
                        return device.toMap().value(QStringLiteral("connected")).toBool();
                    });
                finish(true, connected != m_devices.cend()
                    ? QStringLiteral("Connected to %1.").arg(
                          connected->toMap().value(QStringLiteral("name")).toString())
                    : m_devices.isEmpty()
                        ? QStringLiteral("No discoverable or remembered Bluetooth devices were found.")
                        : QStringLiteral("Found %1 Bluetooth device(s).").arg(m_devices.size()));
            } else if (completed == Operation::Pair) {
                finish(true, QStringLiteral("Paired with %1.").arg(m_pairingName));
                emit pairingFinished(true);
            } else if (completed == Operation::Forget) {
                finish(true, QStringLiteral(
                    "Device forgotten locally. Also choose Forget on the other device."));
                emit forgetFinished(true);
            } else {
                m_discoverable = m_pendingDiscoverable;
                finish(true, m_discoverable
                    ? QStringLiteral("This computer is now discoverable and connectable.")
                    : QStringLiteral("This computer is connectable but hidden."));
            }
        } else {
            QString message = exitStatus == QProcess::NormalExit && exitCode == 126
                ? QStringLiteral("Administrator authorization was cancelled.")
                : error;
            if (message.startsWith(QStringLiteral("ERROR: "))) message.remove(0, 7);
            if (message.isEmpty()) message = QStringLiteral("The Bluetooth operation failed.");
            finish(false, message.left(240));
            if (completed == Operation::Pair) emit pairingFinished(false);
            if (completed == Operation::Forget) emit forgetFinished(false);
        }
    });
}

bool BluetoothController::busy() const { return m_busy; }
bool BluetoothController::discoverable() const { return m_discoverable; }
bool BluetoothController::awaitingConfirmation() const { return m_awaitingConfirmation; }
QString BluetoothController::confirmationCode() const { return m_confirmationCode; }
QString BluetoothController::statusMessage() const { return m_statusMessage; }
bool BluetoothController::statusIsError() const { return m_statusIsError; }
QVariantList BluetoothController::devices() const { return m_devices; }

bool BluetoothController::start(Operation operation, const QStringList &arguments)
{
    if (m_busy) return false;
    QString program;
    QStringList processArguments;
    if (operation == Operation::Scan) {
        program = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_SCAN_COMMAND");
        if (program.isEmpty())
            program = QStringLiteral("/usr/local/libexec/northstar-bluetooth-scan");
        if (!QFileInfo::exists(program)) {
            finish(false, QStringLiteral("The Northstar Bluetooth scanner is not installed."));
            return false;
        }
    } else {
        program = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_AUTH_COMMAND");
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
            processArguments << QStringLiteral("--disable-internal-agent")
                             << QStringLiteral("/usr/local/libexec/northstar-bluetooth-configure");
        }
        if (program.isEmpty()) {
            finish(false, QStringLiteral("Administrator authorization is unavailable."));
            return false;
        }
    }
    processArguments << arguments;
    m_operation = operation;
    m_standardOutput.clear();
    m_busy = true;
    m_statusIsError = false;
    switch (operation) {
    case Operation::Scan:
        m_statusMessage = QStringLiteral("Scanning for Bluetooth devices...");
        break;
    case Operation::Pair:
        m_statusMessage = QStringLiteral("Preparing the protected pairing request...");
        break;
    case Operation::Forget:
        m_statusMessage = QStringLiteral("Preparing to forget the selected device...");
        break;
    case Operation::Discoverability:
        m_statusMessage = QStringLiteral("Changing this computer's Bluetooth visibility...");
        break;
    default:
        break;
    }
    emit stateChanged();
    if (operation != Operation::Scan) {
        m_authorizationPending = true;
        emit authorizationPromptExpected();
    }
    m_process->setProgram(program);
    m_process->setArguments(processArguments);
    m_process->start();
    return true;
}

bool BluetoothController::createRequest(const QByteArray &contents)
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString root = runtime.isEmpty() ? QDir::tempPath() : runtime;
    m_request = new QTemporaryFile(root + QStringLiteral("/northstar-bluetooth-XXXXXX.conf"), this);
    m_request->setAutoRemove(true);
    if (!m_request->open() || !m_request->setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        m_request->deleteLater();
        m_request = nullptr;
        finish(false, QStringLiteral("The protected Bluetooth request could not be created."));
        return false;
    }
    if (m_request->write(contents) != contents.size() || !m_request->flush()) {
        m_request->deleteLater();
        m_request = nullptr;
        finish(false, QStringLiteral("The protected Bluetooth request could not be written."));
        return false;
    }
    return true;
}

bool BluetoothController::refreshDevices()
{
    return start(Operation::Scan, {});
}

bool BluetoothController::pairDevice(const QString &addressHex, const QString &name)
{
    if (m_busy || !AddressHexPattern.match(addressHex).hasMatch()) {
        finish(false, QStringLiteral("Choose a valid scanned Bluetooth device."));
        return false;
    }
    const QByteArray nameBytes = name.toUtf8();
    const QString nameHex = QString::fromLatin1(nameBytes.toHex());
    if (nameBytes.isEmpty() || !NameHexPattern.match(nameHex).hasMatch()) {
        finish(false, QStringLiteral("The selected Bluetooth device name is invalid."));
        return false;
    }
    const QByteArray request = QStringLiteral("protocol=1\naddress_hex=%1\nname_hex=%2\n")
        .arg(addressHex, nameHex).toUtf8();
    if (!createRequest(request)) return false;
    m_pairingName = name;
    return start(Operation::Pair, {QStringLiteral("--pair"), m_request->fileName()});
}

bool BluetoothController::respondToPairing(bool accepted)
{
    if (!m_busy || m_operation != Operation::Pair || !m_awaitingConfirmation)
        return false;
    m_process->write(accepted ? "accept\n" : "reject\n");
    m_process->closeWriteChannel();
    m_awaitingConfirmation = false;
    m_confirmationCode.clear();
    m_statusMessage = accepted
        ? QStringLiteral("Confirm the same number on the other device...")
        : QStringLiteral("Rejecting the Bluetooth pairing request...");
    emit stateChanged();
    return true;
}

bool BluetoothController::forgetDevice(const QString &addressHex)
{
    if (m_busy || !AddressHexPattern.match(addressHex).hasMatch()) {
        finish(false, QStringLiteral("Choose a valid remembered Bluetooth device."));
        return false;
    }
    const QByteArray request = QStringLiteral("protocol=1\naddress_hex=%1\n")
        .arg(addressHex).toUtf8();
    if (!createRequest(request)) return false;
    return start(Operation::Forget, {QStringLiteral("--forget"), m_request->fileName()});
}

bool BluetoothController::setDiscoverable(bool enabled)
{
    if (m_busy) return false;
    m_pendingDiscoverable = enabled;
    return start(Operation::Discoverability,
                 {QStringLiteral("--discoverable"),
                  enabled ? QStringLiteral("on") : QStringLiteral("off")});
}

void BluetoothController::parseScan(const QByteArray &output)
{
    QVariantList parsed;
    bool discoverable = false;
    for (const QByteArray &line : output.split('\n')) {
        if (line == "discoverable=1") discoverable = true;
        if (!line.startsWith("device=")) continue;
        const QList<QByteArray> fields = line.mid(7).split('|');
        if (fields.size() != 5) continue;
        const QString addressHex = QString::fromLatin1(fields.at(0));
        const QString nameHex = QString::fromLatin1(fields.at(1));
        if (!AddressHexPattern.match(addressHex).hasMatch()
            || !NameHexPattern.match(nameHex).hasMatch()
            || (fields.at(2) != "0" && fields.at(2) != "1")
            || (fields.at(3) != "0" && fields.at(3) != "1")
            || (fields.at(4) != "0" && fields.at(4) != "1")) continue;
        QString name = QString::fromUtf8(QByteArray::fromHex(fields.at(1)));
        if (name.trimmed().isEmpty()) name = QStringLiteral("Unnamed Bluetooth device");
        parsed.append(QVariantMap{{QStringLiteral("name"), name},
                                  {QStringLiteral("addressHex"), addressHex},
                                  {QStringLiteral("remembered"), fields.at(2) == "1"},
                                  {QStringLiteral("paired"), fields.at(3) == "1"},
                                  {QStringLiteral("connected"), fields.at(4) == "1"}});
    }
    std::sort(parsed.begin(), parsed.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap left = a.toMap();
        const QVariantMap right = b.toMap();
        if (left.value(QStringLiteral("connected")).toBool()
            != right.value(QStringLiteral("connected")).toBool())
            return left.value(QStringLiteral("connected")).toBool();
        if (left.value(QStringLiteral("paired")).toBool()
            != right.value(QStringLiteral("paired")).toBool())
            return left.value(QStringLiteral("paired")).toBool();
        if (left.value(QStringLiteral("remembered")).toBool()
            != right.value(QStringLiteral("remembered")).toBool())
            return left.value(QStringLiteral("remembered")).toBool();
        return left.value(QStringLiteral("name")).toString().localeAwareCompare(
            right.value(QStringLiteral("name")).toString()) < 0;
    });
    m_discoverable = discoverable;
    m_devices = parsed;
    emit devicesChanged();
}

void BluetoothController::finish(bool success, const QString &message)
{
    if (m_authorizationPending) {
        m_authorizationPending = false;
        emit authorizationCompleted();
    }
    m_busy = false;
    m_statusIsError = !success;
    m_statusMessage = message;
    m_awaitingConfirmation = false;
    m_confirmationCode.clear();
    m_operation = Operation::None;
    if (m_request) {
        m_request->deleteLater();
        m_request = nullptr;
    }
    emit stateChanged();
}

void BluetoothController::processOutput(bool flushRemainder)
{
    for (;;) {
        const qsizetype newline = m_standardOutput.indexOf('\n');
        if (newline < 0 && !flushRemainder) return;
        if (newline < 0 && m_standardOutput.isEmpty()) return;
        const QByteArray line = newline < 0
            ? m_standardOutput : m_standardOutput.left(newline);
        m_standardOutput.remove(0, newline < 0 ? m_standardOutput.size() : newline + 1);
        if (line == "NORTHSTAR_BLUETOOTH_AUTHORIZED=1" && m_authorizationPending) {
            m_authorizationPending = false;
            emit authorizationCompleted();
        } else if (line.startsWith("NORTHSTAR_BLUETOOTH_CONFIRM=")) {
            const QString code = QString::fromLatin1(line.mid(28));
            if (ConfirmationPattern.match(code).hasMatch()) {
                m_confirmationCode = code;
                m_awaitingConfirmation = true;
                m_statusMessage = QStringLiteral("Confirm that %1 appears on both devices.").arg(code);
                emit stateChanged();
                emit pairingConfirmationRequested();
            }
        }
    }
}
