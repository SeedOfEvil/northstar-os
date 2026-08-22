#include "wificontroller.h"

#include <algorithm>

#include <QDir>
#include <QHash>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {
const QRegularExpression SsidHexPattern(QStringLiteral("^(?:[0-9a-f]{2}){1,32}$"));
}

WifiController::WifiController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_statusMessage(QStringLiteral("Choose Refresh to find nearby wireless networks."))
{
    connect(m_process, &QProcess::started, this, [this]() {
        if (m_operation == Operation::Connect) {
            m_process->write(m_pendingSecret);
            m_process->write("\n", 1);
            m_process->closeWriteChannel();
            clearBytes(m_pendingSecret);
            emit secretsCleared();
        }
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            clearBytes(m_pendingSecret);
            emit secretsCleared();
            finish(false, QStringLiteral("The protected Wi-Fi service could not be started."));
        }
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_busy) return;
        const QByteArray output = m_process->readAllStandardOutput();
        const QString error = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        clearBytes(m_pendingSecret);
        emit secretsCleared();
        const Operation completed = m_operation;
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            if (completed == Operation::Scan) {
                parseScan(output);
                const auto connected = std::find_if(m_networks.cbegin(), m_networks.cend(),
                    [](const QVariant &network) {
                        return network.toMap().value(QStringLiteral("connected")).toBool();
                    });
                finish(true, connected != m_networks.cend()
                    ? QStringLiteral("Connected to %1.").arg(connected->toMap().value(QStringLiteral("ssid")).toString())
                    : m_networks.isEmpty() ? QStringLiteral("No visible wireless networks were found.")
                                           : QStringLiteral("Found %1 wireless network(s).").arg(m_networks.size()));
            } else {
                finish(true, QStringLiteral("Connected. Wi-Fi is ready to use."));
                emit connectionFinished(true);
            }
        } else {
            QString message = exitStatus == QProcess::NormalExit && exitCode == 126
                ? QStringLiteral("Administrator authorization was cancelled.")
                : error;
            if (message.startsWith(QStringLiteral("ERROR: "))) message.remove(0, 7);
            if (message.isEmpty()) message = QStringLiteral("The Wi-Fi operation failed.");
            finish(false, message.left(240));
            if (completed == Operation::Connect) emit connectionFinished(false);
        }
    });
}

bool WifiController::busy() const { return m_busy; }
QString WifiController::statusMessage() const { return m_statusMessage; }
bool WifiController::statusIsError() const { return m_statusIsError; }
QVariantList WifiController::networks() const { return m_networks; }

bool WifiController::start(Operation operation, const QStringList &arguments)
{
    if (m_busy) return false;
    QString program = qEnvironmentVariable("NORTHSTAR_WIFI_AUTH_COMMAND");
    QStringList processArguments;
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        processArguments << QStringLiteral("--disable-internal-agent")
                         << QStringLiteral("/usr/local/libexec/northstar-wifi-configure");
    }
    if (program.isEmpty()) {
        finish(false, QStringLiteral("Administrator authorization is unavailable."));
        return false;
    }
    processArguments << arguments;
    m_operation = operation;
    m_busy = true;
    m_statusIsError = false;
    m_statusMessage = operation == Operation::Scan
        ? QStringLiteral("Scanning for wireless networks...")
        : QStringLiteral("Connecting to the selected network...");
    emit stateChanged();
    m_process->setProgram(program);
    m_process->setArguments(processArguments);
    m_process->start();
    return true;
}

bool WifiController::refreshNetworks()
{
    return start(Operation::Scan, {QStringLiteral("--scan")});
}

bool WifiController::connectNetwork(const QString &ssidHex,
                                    const QString &security,
                                    const QString &passphrase)
{
    if (m_busy) return false;
    if (!SsidHexPattern.match(ssidHex).hasMatch()
        || (security != QStringLiteral("open") && security != QStringLiteral("secured"))) {
        finish(false, QStringLiteral("Choose a valid scanned network."));
        return false;
    }
    if (security == QStringLiteral("secured")
        && (passphrase.size() < 8 || passphrase.size() > 63)) {
        finish(false, QStringLiteral("The Wi-Fi password must contain 8 to 63 characters."));
        return false;
    }
    if (security == QStringLiteral("open") && !passphrase.isEmpty()) {
        finish(false, QStringLiteral("This open network does not use a password."));
        return false;
    }

    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString root = runtime.isEmpty() ? QDir::tempPath() : runtime;
    m_request = new QTemporaryFile(root + QStringLiteral("/northstar-wifi-XXXXXX.conf"), this);
    m_request->setAutoRemove(true);
    if (!m_request->open() || !m_request->setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        m_request->deleteLater();
        m_request = nullptr;
        finish(false, QStringLiteral("The protected Wi-Fi request could not be created."));
        return false;
    }
    const QByteArray request = QStringLiteral("protocol=1\nssid_hex=%1\nsecurity=%2\n")
        .arg(ssidHex, security).toUtf8();
    if (m_request->write(request) != request.size() || !m_request->flush()) {
        m_request->deleteLater();
        m_request = nullptr;
        finish(false, QStringLiteral("The protected Wi-Fi request could not be written."));
        return false;
    }
    m_pendingSecret = passphrase.toUtf8();
    return start(Operation::Connect,
                 {QStringLiteral("--connect"), m_request->fileName()});
}

void WifiController::parseScan(const QByteArray &output)
{
    QVariantList parsed;
    QHash<QString, QVariantMap> strongest;
    const QList<QByteArray> lines = output.split('\n');
    QString connectedHex;
    for (const QByteArray &line : lines) {
        if (line.startsWith("connected=")) {
            const QString candidate = QString::fromLatin1(line.mid(10));
            if (SsidHexPattern.match(candidate).hasMatch()) connectedHex = candidate;
        }
    }
    for (const QByteArray &line : lines) {
        if (!line.startsWith("network=")) continue;
        const QList<QByteArray> fields = line.mid(8).split('|');
        if (fields.size() != 3) continue;
        const QString hex = QString::fromLatin1(fields.at(0));
        const QString security = QString::fromLatin1(fields.at(1));
        bool signalOk = false;
        const int signal = fields.at(2).toInt(&signalOk);
        if (!SsidHexPattern.match(hex).hasMatch() || !signalOk
            || (security != QStringLiteral("open") && security != QStringLiteral("secured"))) continue;
        const QString ssid = QString::fromUtf8(QByteArray::fromHex(fields.at(0)));
        if (ssid.isEmpty()) continue;
        const QVariantMap item{{QStringLiteral("ssid"), ssid},
                               {QStringLiteral("ssidHex"), hex},
                               {QStringLiteral("security"), security},
                               {QStringLiteral("secured"), security == QStringLiteral("secured")},
                               {QStringLiteral("connected"), hex == connectedHex},
                               {QStringLiteral("signal"), signal}};
        if (!strongest.contains(hex) || strongest.value(hex).value(QStringLiteral("signal")).toInt() < signal)
            strongest.insert(hex, item);
    }
    QList<QVariantMap> items = strongest.values();
    std::sort(items.begin(), items.end(), [](const QVariantMap &a, const QVariantMap &b) {
        if (a.value(QStringLiteral("connected")).toBool() != b.value(QStringLiteral("connected")).toBool())
            return a.value(QStringLiteral("connected")).toBool();
        return a.value(QStringLiteral("signal")).toInt() > b.value(QStringLiteral("signal")).toInt();
    });
    for (const QVariantMap &item : items) parsed.append(item);
    m_networks = parsed;
    emit networksChanged();
}

void WifiController::finish(bool success, const QString &message)
{
    m_busy = false;
    m_statusIsError = !success;
    m_statusMessage = message;
    m_operation = Operation::None;
    if (m_request) {
        m_request->deleteLater();
        m_request = nullptr;
    }
    emit stateChanged();
}

void WifiController::clearBytes(QByteArray &bytes)
{
    bytes.fill('\0');
    bytes.clear();
}
