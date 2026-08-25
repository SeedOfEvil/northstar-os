#pragma once

#include <functional>

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>

class InputController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool mouseAvailable READ mouseAvailable NOTIFY changed)
    Q_PROPERTY(bool touchpadAvailable READ touchpadAvailable NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY changed)

public:
    using RequestFunction = std::function<bool(const QString &, const QJsonObject &,
                                               QJsonValue *, QString *)>;

    explicit InputController(QObject *parent = nullptr, QString configPath = {},
                             RequestFunction requestFunction = {});

    bool available() const;
    bool mouseAvailable() const;
    bool touchpadAvailable() const;
    QString statusMessage() const;
    QStringList deviceNames() const;

    int mouseSpeed() const;
    int touchpadSpeed() const;
    bool mouseNaturalScroll() const;
    bool touchpadNaturalScroll() const;
    bool tapToClick() const;
    bool disableWhileTyping() const;
    QString clickMethod() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setMouseSpeed(int value);
    Q_INVOKABLE bool setTouchpadSpeed(int value);
    Q_INVOKABLE bool setMouseNaturalScroll(bool enabled);
    Q_INVOKABLE bool setTouchpadNaturalScroll(bool enabled);
    Q_INVOKABLE bool setTapToClick(bool enabled);
    Q_INVOKABLE bool setDisableWhileTyping(bool enabled);
    Q_INVOKABLE bool setClickMethod(const QString &method);

signals:
    void changed();

private:
    static QString defaultConfigPath();
    bool requestInventory(QJsonValue *response, QString *error) const;
    bool writeOption(const QString &key, const QString &value);
    QString readOption(const QString &key, const QString &fallback) const;

    QString m_configPath;
    RequestFunction m_requestFunction;
    QStringList m_deviceNames;
    bool m_available = false;
    bool m_mouseAvailable = false;
    bool m_touchpadAvailable = false;
    QString m_statusMessage = QStringLiteral("No pointing devices detected");
};
