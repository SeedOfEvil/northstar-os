#pragma once

#include <functional>

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QVariantList>

class WindowController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    using RequestFunction = std::function<bool(const QString &method,
                                               const QJsonObject &data,
                                               QJsonValue *response,
                                               QString *error)>;

    explicit WindowController(QObject *parent = nullptr, RequestFunction requestFunction = {});

    QVariantList windows() const;
    bool available() const;
    QString statusMessage() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool activateWindow(int viewId);
    Q_INVOKABLE bool toggleMinimize(int viewId);

signals:
    void windowsChanged();
    void availabilityChanged();
    void statusMessageChanged();

private:
    bool request(const QString &method,
                 const QJsonObject &data,
                 QJsonValue *response,
                 QString *error);
    bool sendSocketRequest(const QString &method,
                           const QJsonObject &data,
                           QJsonValue *response,
                           QString *error) const;
    void setRequestStatus(bool available, const QString &message);
    bool setMinimized(int viewId, bool minimized);
    bool minimizedStateFor(int viewId, bool *minimized) const;

    QVariantList m_windows;
    RequestFunction m_requestFunction;
    bool m_available = false;
    QString m_statusMessage;
};
