#pragma once

#include <QList>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

// One entry from a .desktop file's Actions= list, such as Firefox's
// "Open a New Private Window". The desktop entry specification calls these
// additional actions; a desktop offers them on a right-click.
struct DesktopAction
{
    QString id;
    QString name;
    QString exec;
    QString icon;
};

inline bool operator==(const DesktopAction &left, const DesktopAction &right)
{
    return left.id == right.id && left.name == right.name && left.exec == right.exec
        && left.icon == right.icon;
}

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
    QList<DesktopAction> actions;
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
        && left.actions == right.actions
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

    // The command for one of an application's additional actions. An unknown
    // action is refused rather than falling back to launching the
    // application, because a caller asking for "New Private Window" would not
    // want an ordinary window instead.
    bool actionLaunchSpec(const QString &desktopId, const QString &actionId, QString *program,
                          QStringList *arguments) const;

    static QStringList defaultApplicationDirectories();

signals:
    void applicationsChanged();

private:
    static QVariantList toVariantList(const QList<DesktopApplication> &entries);
    static bool readDesktopEntry(const QString &path, const QString &desktopId, DesktopApplication *application);
    static QStringList tokenizeExec(const QString &exec);
    static QStringList expandExec(const DesktopApplication &application);
    static QStringList expandActionExec(const DesktopApplication &application,
                                        const DesktopAction &action);
    void scheduleReload();
    void refreshWatchPaths();

    QStringList m_applicationDirectories;
    QList<DesktopApplication> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
};
