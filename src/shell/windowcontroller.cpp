#include "windowcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QHash>
#include <QStringList>
#include <QVariantMap>

#include <utility>

namespace {

QStringList socketCandidates()
{
    QStringList candidates;
    const QString configuredSocket = qEnvironmentVariable("WAYFIRE_SOCKET").trimmed();
    if (!configuredSocket.isEmpty()) {
        candidates.append(configuredSocket);
    }

    QStringList runtimeDirectories;
    const QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR").trimmed();
    if (!runtimeDirectory.isEmpty()) {
        runtimeDirectories.append(runtimeDirectory);
    }
    runtimeDirectories.append(QStringLiteral("/tmp"));

    for (const QString &directoryPath : runtimeDirectories) {
        const QDir directory(directoryPath);
        const QStringList entries = directory.entryList(
            {QStringLiteral("wayfire-wayland-*.socket"), QStringLiteral("wayfire-*.socket")},
            QDir::System | QDir::Readable | QDir::NoDotAndDotDot,
            QDir::Name);
        for (const QString &entry : entries) {
            candidates.append(directory.filePath(entry));
        }
    }

    candidates.removeDuplicates();
    return candidates;
}

QByteArray frameFor(const QByteArray &payload)
{
    const quint32 length = static_cast<quint32>(payload.size());
    QByteArray frame;
    frame.resize(4);
    frame[0] = static_cast<char>(length & 0xffU);
    frame[1] = static_cast<char>((length >> 8U) & 0xffU);
    frame[2] = static_cast<char>((length >> 16U) & 0xffU);
    frame[3] = static_cast<char>((length >> 24U) & 0xffU);
    frame.append(payload);
    return frame;
}

quint32 lengthFrom(const QByteArray &header)
{
    return static_cast<quint32>(static_cast<unsigned char>(header.at(0)))
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(1))) << 8U)
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(2))) << 16U)
        | (static_cast<quint32>(static_cast<unsigned char>(header.at(3))) << 24U);
}

bool readExact(QLocalSocket *socket, qint64 length, QByteArray *result, QString *error)
{
    if (socket == nullptr || result == nullptr || length < 0) {
        if (error != nullptr) {
            *error = QStringLiteral("invalid Wayfire IPC read request");
        }
        return false;
    }

    result->clear();
    while (result->size() < length) {
        if (socket->bytesAvailable() == 0 && !socket->waitForReadyRead(750)) {
            if (error != nullptr) {
                *error = QStringLiteral("timed out waiting for Wayfire IPC response");
            }
            return false;
        }
        result->append(socket->read(length - result->size()));
    }
    return true;
}

} // namespace

WindowController::WindowController(QObject *parent, RequestFunction requestFunction)
    : QObject(parent)
    , m_requestFunction(std::move(requestFunction))
{
}

QVariantList WindowController::windows() const
{
    return m_windows;
}

QVariantList WindowController::applicationGroups() const
{
    return m_applicationGroups;
}

bool WindowController::available() const
{
    return m_available;
}

QString WindowController::statusMessage() const
{
    return m_statusMessage;
}

bool WindowController::refresh()
{
    QJsonValue response;
    QString error;
    if (!request(QStringLiteral("window-rules/list-views"), {}, &response, &error)) {
        setRequestStatus(false, error);
        return false;
    }

    QJsonArray views;
    if (response.isArray()) {
        views = response.toArray();
    } else if (response.isObject()) {
        views = response.toObject().value(QStringLiteral("views")).toArray();
    }

    QVariantList nextWindows;
    for (const QJsonValue &value : views) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject view = value.toObject();
        const qint64 pid = view.value(QStringLiteral("pid")).toVariant().toLongLong();
        const int viewId = view.value(QStringLiteral("id")).toInt(-1);
        const QString role = view.value(QStringLiteral("role")).toString();
        // The shell's own panels, dock, and background are excluded by their
        // role, which is what the compositor reports them as. Excluding
        // everything from the shell process as well took its ordinary windows
        // with them: Settings, Files, and the Software Center are toplevels
        // from the same process, so minimising one sent it somewhere the dock
        // had been told to ignore, and it could not be got back.
        if (viewId < 0 || pid <= 0
            || !view.value(QStringLiteral("mapped")).toBool(false)
            || role == QStringLiteral("desktop-environment")) {
            continue;
        }

        const QString title = view.value(QStringLiteral("title")).toString().trimmed();
        const QString appId = view.value(QStringLiteral("app-id")).toString().trimmed();
        const QString label = !title.isEmpty() ? title : (!appId.isEmpty() ? appId : QStringLiteral("Application"));
        const bool active = view.value(QStringLiteral("focused")).toBool(false)
            || view.value(QStringLiteral("active")).toBool(false)
            || view.value(QStringLiteral("activated")).toBool(false)
            || view.value(QStringLiteral("focus")).toBool(false)
            || view.value(QStringLiteral("is-focused")).toBool(false);
        nextWindows.append(QVariantMap{
            {QStringLiteral("viewId"), viewId},
            {QStringLiteral("pid"), pid},
            {QStringLiteral("title"), label},
            {QStringLiteral("appId"), appId},
            {QStringLiteral("minimized"), view.value(QStringLiteral("minimized")).toBool(false)},
            {QStringLiteral("active"), active},
        });
    }

    m_windows = nextWindows;
    rebuildApplicationGroups();
    emit windowsChanged();
    setRequestStatus(true, QStringLiteral("%1 open app%2")
        .arg(m_windows.size())
        .arg(m_windows.size() == 1 ? QString() : QStringLiteral("s")));
    return true;
}

bool WindowController::activateWindow(int viewId)
{
    QJsonValue response;
    QString error;
    if (!request(QStringLiteral("window-rules/focus-view"),
                 QJsonObject{{QStringLiteral("id"), viewId}},
                 &response,
                 &error)) {
        setRequestStatus(false, error);
        return false;
    }

    setRequestStatus(true, QStringLiteral("Focused application"));
    refresh();
    return true;
}

bool WindowController::toggleMinimize(int viewId)
{
    bool minimized = false;
    if (!minimizedStateFor(viewId, &minimized)) {
        refresh();
        if (!minimizedStateFor(viewId, &minimized)) {
            setRequestStatus(false, QStringLiteral("Application is no longer available"));
            return false;
        }
    }
    return setMinimized(viewId, !minimized);
}

bool WindowController::request(const QString &method,
                               const QJsonObject &data,
                               QJsonValue *response,
                               QString *error)
{
    if (m_requestFunction) {
        return m_requestFunction(method, data, response, error);
    }
    return sendSocketRequest(method, data, response, error);
}

bool WindowController::sendSocketRequest(const QString &method,
                                          const QJsonObject &data,
                                          QJsonValue *response,
                                          QString *error) const
{
    QJsonObject message{{QStringLiteral("method"), method}};
    if (!data.isEmpty()) {
        message.insert(QStringLiteral("data"), data);
    }
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    const QByteArray frame = frameFor(payload);

    QString lastError = QStringLiteral("Wayfire IPC socket was not found");
    for (const QString &socketPath : socketCandidates()) {
        QLocalSocket socket;
        socket.connectToServer(socketPath);
        if (!socket.waitForConnected(300)) {
            lastError = QStringLiteral("could not connect to Wayfire IPC socket %1").arg(socketPath);
            continue;
        }
        if (socket.write(frame) != frame.size() || !socket.waitForBytesWritten(500)) {
            lastError = QStringLiteral("could not send Wayfire IPC request");
            continue;
        }

        QByteArray header;
        if (!readExact(&socket, 4, &header, &lastError)) {
            continue;
        }
        const quint32 responseLength = lengthFrom(header);
        if (responseLength > 16U * 1024U * 1024U) {
            lastError = QStringLiteral("Wayfire IPC response was too large");
            continue;
        }
        QByteArray responseBytes;
        if (!readExact(&socket, responseLength, &responseBytes, &lastError)) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            lastError = QStringLiteral("invalid Wayfire IPC response: %1").arg(parseError.errorString());
            continue;
        }
        const QJsonValue parsed = responseDocument.isArray()
            ? QJsonValue(responseDocument.array())
            : QJsonValue(responseDocument.object());
        if (parsed.isObject() && parsed.toObject().contains(QStringLiteral("error"))) {
            lastError = parsed.toObject().value(QStringLiteral("error")).toString();
            continue;
        }
        if (response != nullptr) {
            *response = parsed;
        }
        return true;
    }

    if (error != nullptr) {
        *error = lastError;
    }
    return false;
}

void WindowController::setRequestStatus(bool available, const QString &message)
{
    if (m_available != available) {
        m_available = available;
        emit availabilityChanged();
    }
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

bool WindowController::setMinimized(int viewId, bool minimized)
{
    QJsonValue response;
    QString error;
    if (!request(QStringLiteral("wm-actions/set-minimized"),
                 QJsonObject{
                     {QStringLiteral("view_id"), viewId},
                     {QStringLiteral("state"), minimized},
                 },
                 &response,
                 &error)) {
        setRequestStatus(false, error);
        return false;
    }

    setRequestStatus(true, minimized ? QStringLiteral("Application minimized") : QStringLiteral("Application restored"));
    refresh();
    return true;
}

bool WindowController::minimizedStateFor(int viewId, bool *minimized) const
{
    if (minimized == nullptr) {
        return false;
    }
    for (const QVariant &entry : m_windows) {
        const QVariantMap window = entry.toMap();
        if (window.value(QStringLiteral("viewId")).toInt() == viewId) {
            *minimized = window.value(QStringLiteral("minimized")).toBool();
            return true;
        }
    }
    return false;
}

QString WindowController::applicationIdentity(const QString &appId, const QString &title)
{
    QString identity = appId.trimmed().toLower();
    if (identity.endsWith(QStringLiteral(".desktop"))) {
        identity.chop(8);
    }
    const QString descriptor = identity + QLatin1Char(' ') + title.trimmed().toLower();
    if (descriptor.contains(QStringLiteral("qterminal"))
        || descriptor.contains(QStringLiteral("terminal"))) {
        return QStringLiteral("qterminal");
    }
    if (descriptor.contains(QStringLiteral("firefox"))) {
        return QStringLiteral("firefox");
    }
    if (!identity.isEmpty()) {
        return identity;
    }

    QString fallback = title.trimmed().toLower();
    fallback.replace(QLatin1Char(' '), QLatin1Char('-'));
    return fallback.isEmpty() ? QStringLiteral("application") : fallback;
}

void WindowController::rebuildApplicationGroups()
{
    QVariantList groups;
    QHash<QString, int> indexes;
    for (const QVariant &entry : std::as_const(m_windows)) {
        const QVariantMap window = entry.toMap();
        const QString identity = applicationIdentity(
            window.value(QStringLiteral("appId")).toString(),
            window.value(QStringLiteral("title")).toString());
        if (!indexes.contains(identity)) {
            indexes.insert(identity, groups.size());
            groups.append(QVariantMap{
                {QStringLiteral("identity"), identity},
                {QStringLiteral("desktopId"), identity},
                {QStringLiteral("title"), window.value(QStringLiteral("title"))},
                {QStringLiteral("appId"), window.value(QStringLiteral("appId"))},
                {QStringLiteral("windows"), QVariantList{}},
                {QStringLiteral("count"), 0},
                {QStringLiteral("active"), false},
                {QStringLiteral("allMinimized"), true},
            });
        }

        const int groupIndex = indexes.value(identity);
        QVariantMap group = groups.at(groupIndex).toMap();
        QVariantList windows = group.value(QStringLiteral("windows")).toList();
        windows.append(window);
        group.insert(QStringLiteral("windows"), windows);
        group.insert(QStringLiteral("count"), windows.size());
        group.insert(QStringLiteral("active"), group.value(QStringLiteral("active")).toBool()
            || window.value(QStringLiteral("active")).toBool());
        group.insert(QStringLiteral("allMinimized"), group.value(QStringLiteral("allMinimized")).toBool()
            && window.value(QStringLiteral("minimized")).toBool());
        groups[groupIndex] = group;
    }
    m_applicationGroups = groups;
}
