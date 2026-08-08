#pragma once

#include <functional>

#include <QObject>
#include <QString>

class SessionController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY statusChanged)
    Q_PROPERTY(QString state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString waylandDisplay READ waylandDisplay NOTIFY statusChanged)
    Q_PROPERTY(qint64 supervisorPid READ supervisorPid NOTIFY statusChanged)
    Q_PROPERTY(qint64 compositorPid READ compositorPid NOTIFY statusChanged)
    Q_PROPERTY(qint64 shellPid READ shellPid NOTIFY statusChanged)
    Q_PROPERTY(int restartCount READ restartCount NOTIFY statusChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent NOTIFY statusChanged)

public:
    using SignalFunction = std::function<int(qint64 pid, int signal)>;

    explicit SessionController(QObject *parent = nullptr);
    SessionController(const QString &statusFile,
                      const QString &controlFile,
                      qint64 expectedSupervisorPid,
                      QObject *parent = nullptr,
                      SignalFunction signalFunction = {});

    bool available() const;
    QString state() const;
    QString waylandDisplay() const;
    qint64 supervisorPid() const;
    qint64 compositorPid() const;
    qint64 shellPid() const;
    int restartCount() const;
    QString lastEvent() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool requestEndSession();

signals:
    void statusChanged();

private:
    void clearStatus();

    QString m_statusFile;
    QString m_controlFile;
    qint64 m_expectedSupervisorPid = 0;
    SignalFunction m_signalFunction;
    bool m_available = false;
    QString m_state = QStringLiteral("Not supervised");
    QString m_waylandDisplay;
    qint64 m_supervisorPid = 0;
    qint64 m_compositorPid = 0;
    qint64 m_shellPid = 0;
    int m_restartCount = 0;
    QString m_lastEvent;
};
