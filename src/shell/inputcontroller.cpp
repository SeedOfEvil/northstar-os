#include "inputcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QtMath>

#include <utility>

namespace {

QStringList socketCandidates()
{
    QStringList candidates;
    const QString configured = qEnvironmentVariable("WAYFIRE_SOCKET").trimmed();
    if (!configured.isEmpty()) {
        candidates.append(configured);
    }
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR").trimmed();
    for (const QString &root : QStringList{runtime, QStringLiteral("/tmp")}) {
        if (root.isEmpty()) {
            continue;
        }
        const QDir directory(root);
        for (const QString &name : directory.entryList(
                 {QStringLiteral("wayfire-wayland-*.socket"), QStringLiteral("wayfire-*.socket")},
                 QDir::System | QDir::Readable | QDir::NoDotAndDotDot, QDir::Name)) {
            candidates.append(directory.filePath(name));
        }
    }
    candidates.removeDuplicates();
    return candidates;
}

quint32 responseLength(const QByteArray &header)
{
    return static_cast<quint32>(static_cast<unsigned char>(header.at(0)))
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(1))) << 8U)
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(2))) << 16U)
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(3))) << 24U);
}

bool socketRequest(const QString &method, const QJsonObject &data,
                   QJsonValue *response, QString *error)
{
    QJsonObject message{{QStringLiteral("method"), method}};
    if (!data.isEmpty()) {
        message.insert(QStringLiteral("data"), data);
    }
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray frame(4, Qt::Uninitialized);
    const quint32 size = static_cast<quint32>(payload.size());
    for (int index = 0; index < 4; ++index) {
        frame[index] = static_cast<char>((size >> (index * 8)) & 0xffU);
    }
    frame.append(payload);

    QString lastError = QStringLiteral("Wayfire input service is unavailable");
    for (const QString &path : socketCandidates()) {
        QLocalSocket socket;
        socket.connectToServer(path);
        if (!socket.waitForConnected(500)) {
            continue;
        }
        if (socket.write(frame) != frame.size() || !socket.waitForBytesWritten(500)) {
            lastError = QStringLiteral("Wayfire input request could not be sent");
            continue;
        }
        QByteArray header;
        while (header.size() < 4
               && (socket.bytesAvailable() > 0 || socket.waitForReadyRead(750))) {
            header.append(socket.read(4 - header.size()));
        }
        if (header.size() != 4) {
            lastError = QStringLiteral("Wayfire input response timed out");
            continue;
        }
        const quint32 length = responseLength(header);
        QByteArray body;
        while (body.size() < static_cast<int>(length)
               && (socket.bytesAvailable() > 0 || socket.waitForReadyRead(750))) {
            body.append(socket.read(length - body.size()));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            lastError = QStringLiteral("Wayfire returned an invalid input response");
            continue;
        }
        *response = document.isArray() ? QJsonValue(document.array())
                                       : QJsonValue(document.object());
        return true;
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

bool persistInputOption(const QString &path, const QString &key, const QString &value)
{
    static const QStringList allowed{
        QStringLiteral("mouse_cursor_speed"), QStringLiteral("touchpad_cursor_speed"),
        QStringLiteral("mouse_natural_scroll"), QStringLiteral("natural_scroll"),
        QStringLiteral("tap_to_click"), QStringLiteral("disable_touchpad_while_typing"),
        QStringLiteral("click_method")};
    if (!allowed.contains(key)) {
        return false;
    }
    const QFileInfo info(path);
    if (info.isSymLink() || !QDir().mkpath(info.absolutePath())) {
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
    const QRegularExpression section(QStringLiteral(R"(^\s*\[[^]]+\]\s*$)"));
    const QRegularExpression assignment(QStringLiteral(R"(^\s*%1\s*=)").arg(
        QRegularExpression::escape(key)), QRegularExpression::CaseInsensitiveOption);
    QStringList output;
    bool inInput = false;
    bool foundSection = false;
    bool written = false;
    for (const QString &line : content.split(QLatin1Char('\n'))) {
        if (section.match(line).hasMatch()) {
            if (inInput && !written) {
                output.append(QStringLiteral("%1 = %2").arg(key, value));
                written = true;
            }
            inInput = line.trimmed().compare(QStringLiteral("[input]"), Qt::CaseInsensitive) == 0;
            foundSection = foundSection || inInput;
        }
        if (inInput && assignment.match(line).hasMatch()) {
            if (!written) {
                output.append(QStringLiteral("%1 = %2").arg(key, value));
                written = true;
            }
            continue;
        }
        output.append(line);
    }
    if (inInput && !written) {
        output.append(QStringLiteral("%1 = %2").arg(key, value));
    } else if (!foundSection) {
        if (!output.isEmpty() && !output.constLast().isEmpty()) {
            output.append(QString());
        }
        output.append(QStringLiteral("[input]"));
        output.append(QStringLiteral("%1 = %2").arg(key, value));
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(output.join(QLatin1Char('\n')).toUtf8()) < 0) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace

InputController::InputController(QObject *parent, QString configPath,
                                 RequestFunction requestFunction)
    : QObject(parent)
    , m_configPath(configPath.trimmed().isEmpty() ? defaultConfigPath() : configPath)
    , m_requestFunction(requestFunction ? std::move(requestFunction) : socketRequest)
{
    refresh();
}

bool InputController::available() const { return m_available; }
bool InputController::mouseAvailable() const { return m_mouseAvailable; }
bool InputController::touchpadAvailable() const { return m_touchpadAvailable; }
QString InputController::statusMessage() const { return m_statusMessage; }
QStringList InputController::deviceNames() const { return m_deviceNames; }
int InputController::mouseSpeed() const { return qRound(readOption("mouse_cursor_speed", "0").toDouble() * 100); }
int InputController::touchpadSpeed() const { return qRound(readOption("touchpad_cursor_speed", "0").toDouble() * 100); }
bool InputController::mouseNaturalScroll() const { return readOption("mouse_natural_scroll", "false") == "true"; }
bool InputController::touchpadNaturalScroll() const { return readOption("natural_scroll", "false") == "true"; }
bool InputController::tapToClick() const { return readOption("tap_to_click", "true") == "true"; }
bool InputController::disableWhileTyping() const { return readOption("disable_touchpad_while_typing", "false") == "true"; }
QString InputController::clickMethod() const { return readOption("click_method", "default"); }

QString InputController::defaultConfigPath()
{
    const QString configured = qEnvironmentVariable("NORTHSTAR_WAYFIRE_CONFIG").trimmed();
    return configured.isEmpty() ? QDir::home().filePath(".config/wayfire.ini") : configured;
}

bool InputController::refresh()
{
    QJsonValue response;
    QString error;
    if (!requestInventory(&response, &error)) {
        m_available = m_mouseAvailable = m_touchpadAvailable = false;
        m_deviceNames.clear();
        m_statusMessage = error;
        emit changed();
        return false;
    }
    m_deviceNames.clear();
    m_mouseAvailable = m_touchpadAvailable = false;
    static const QRegularExpression touchpad(QStringLiteral("touch[ -]?pad|trackpad"),
                                             QRegularExpression::CaseInsensitiveOption);
    for (const QJsonValue &value : response.toArray()) {
        const QJsonObject device = value.toObject();
        if (device.value("type").toString() != QStringLiteral("pointer")
            || !device.value("enabled").toBool(true)) {
            continue;
        }
        const QString name = device.value("name").toString().trimmed();
        m_deviceNames.append(name);
        if (touchpad.match(name).hasMatch()) {
            m_touchpadAvailable = true;
        } else {
            m_mouseAvailable = true;
        }
    }
    m_available = m_mouseAvailable || m_touchpadAvailable;
    m_statusMessage = m_available
        ? QStringLiteral("%1 pointing device%2 detected").arg(m_deviceNames.size())
              .arg(m_deviceNames.size() == 1 ? QString() : QStringLiteral("s"))
        : QStringLiteral("No pointing devices detected");
    emit changed();
    return true;
}

bool InputController::requestInventory(QJsonValue *response, QString *error) const
{
    return m_requestFunction(QStringLiteral("input/list-devices"), {}, response, error);
}

QString InputController::readOption(const QString &key, const QString &fallback) const
{
    QSettings settings(m_configPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("input/") + key, fallback).toString().trimmed().toLower();
}

bool InputController::writeOption(const QString &key, const QString &value)
{
    if (!persistInputOption(m_configPath, key, value)) {
        m_statusMessage = QStringLiteral("Input setting could not be saved");
        emit changed();
        return false;
    }
    m_statusMessage = QStringLiteral("Input setting applied");
    emit changed();
    return readOption(key, QString()) == value;
}

bool InputController::setMouseSpeed(int value) { return value >= -100 && value <= 100 && writeOption("mouse_cursor_speed", QString::number(value / 100.0, 'f', 2)); }
bool InputController::setTouchpadSpeed(int value) { return value >= -100 && value <= 100 && writeOption("touchpad_cursor_speed", QString::number(value / 100.0, 'f', 2)); }
bool InputController::setMouseNaturalScroll(bool enabled) { return writeOption("mouse_natural_scroll", enabled ? "true" : "false"); }
bool InputController::setTouchpadNaturalScroll(bool enabled) { return writeOption("natural_scroll", enabled ? "true" : "false"); }
bool InputController::setTapToClick(bool enabled) { return writeOption("tap_to_click", enabled ? "true" : "false"); }
bool InputController::setDisableWhileTyping(bool enabled) { return writeOption("disable_touchpad_while_typing", enabled ? "true" : "false"); }
bool InputController::setClickMethod(const QString &method)
{
    return QStringList{QStringLiteral("default"), QStringLiteral("button-areas"),
                       QStringLiteral("clickfinger")}.contains(method)
        && writeOption(QStringLiteral("click_method"), method);
}
