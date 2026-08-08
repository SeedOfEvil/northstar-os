#pragma once

#include <functional>

#include <QObject>
#include <QString>

class PowerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY statusChanged)

public:
    using PowerFunction = std::function<bool(const QString &action, QString *error)>;

    explicit PowerController(QObject *parent = nullptr,
                              PowerFunction powerFunction = {},
                              QString helperPath = {});

    bool available() const;
    bool busy() const;
    QString statusMessage() const;
    QString lastAction() const;

    Q_INVOKABLE bool requestRestart();
    Q_INVOKABLE bool requestShutdown();

signals:
    void statusChanged();

private:
    bool request(const QString &action);
    bool runHelper(const QString &action, QString *error) const;

    QString m_helperPath;
    PowerFunction m_powerFunction;
    bool m_available = false;
    bool m_busy = false;
    QString m_statusMessage;
    QString m_lastAction;
};
