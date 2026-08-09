#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QFileSystemWatcher;
class QFileInfo;
class QTimer;

class DesktopItemsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString desktopPath READ desktopPath CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit DesktopItemsController(QObject *parent = nullptr, QString homePath = {});

    QVariantList entries() const;
    QString desktopPath() const;
    QString errorMessage() const;
    bool available() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool requestOpen(const QString &path);
    Q_INVOKABLE bool requestOpenWith(const QString &path);

signals:
    void entriesChanged();
    void errorMessageChanged();
    void availableChanged();
    void openPathRequested(const QString &path, bool isDirectory, bool isLaunchable);
    void openWithRequested(const QString &path);

private:
    static QString normalizedPath(const QString &path);
    static QString canonicalOrNormalizedPath(const QString &path);
    static bool pathMatchesRoot(const QString &path, const QString &root);
    static bool isLaunchable(const QString &path, const QFileInfo &info);
    bool isWithinHome(const QString &path) const;
    bool resolveDesktopItem(const QString &path, QString *resolvedPath, QFileInfo *info) const;
    void refreshWatcher();
    void scheduleRefresh();
    void setErrorMessage(const QString &message);

    QString m_homePath;
    QString m_desktopPath;
    QString m_errorMessage;
    QVariantList m_entries;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_refreshTimer = nullptr;
    bool m_available = false;
};
