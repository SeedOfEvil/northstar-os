#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class DesktopLayoutController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap positions READ positions NOTIFY positionsChanged)

public:
    explicit DesktopLayoutController(QObject *parent = nullptr, QString settingsPath = {});

    QVariantMap positions() const;

    Q_INVOKABLE QVariantMap positionFor(const QString &path) const;
    Q_INVOKABLE bool setPosition(const QString &path, qreal x, qreal y);
    Q_INVOKABLE bool clearPosition(const QString &path);
    Q_INVOKABLE void reset();

signals:
    void positionsChanged();

private:
    static QString normalizedPath(const QString &path);
    static QString settingsKeyFor(const QString &path);
    static QString pathForSettingsKey(const QString &key);
    void loadPreferences();
    bool savePreferences() const;

    QString m_settingsPath;
    QVariantMap m_positions;
};
