#pragma once

#include <QList>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

struct DesktopApplication
{
    QString desktopId;
    QString name;
    QString genericName;
    QString exec;
    QString icon;
    QStringList categories;
    QStringList mimeTypes;
    QString sourcePath;
    bool launchable = false;
};

inline bool operator==(const DesktopApplication &left, const DesktopApplication &right)
{
    return left.desktopId == right.desktopId
        && left.name == right.name
        && left.genericName == right.genericName
        && left.exec == right.exec
        && left.icon == right.icon
        && left.categories == right.categories
        && left.mimeTypes == right.mimeTypes
        && left.sourcePath == right.sourcePath
        && left.launchable == right.launchable;
}

class ApplicationCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList applications READ applications NOTIFY applicationsChanged)

public:
    explicit ApplicationCatalog(QStringList applicationDirectories = {}, QObject *parent = nullptr);

    QList<DesktopApplication> entries() const;
    QVariantList applications() const;
    QVariantList searchApplications(const QString &query) const;
    QStringList applicationIds() const;

    bool reload();
    bool launchSpec(const QString &desktopId, QString *program, QStringList *arguments) const;

    static QStringList defaultApplicationDirectories();

signals:
    void applicationsChanged();

private:
    static QVariantList toVariantList(const QList<DesktopApplication> &entries);
    static bool readDesktopEntry(const QString &path, const QString &desktopId, DesktopApplication *application);
    static QStringList tokenizeExec(const QString &exec);
    static QStringList expandExec(const DesktopApplication &application);
    void scheduleReload();
    void refreshWatchPaths();

    QStringList m_applicationDirectories;
    QList<DesktopApplication> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
};
