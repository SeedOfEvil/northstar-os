#pragma once

#include <QList>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

struct BundleProvenance
{
    QString source;
    QString package;
    QString revision;
};

inline bool operator==(const BundleProvenance &left, const BundleProvenance &right)
{
    return left.source == right.source
        && left.package == right.package
        && left.revision == right.revision;
}

struct BundleApplication
{
    QString bundleId;
    QString desktopId;
    QString name;
    QString version;
    QString executable;
    QString icon;
    QStringList categories;
    QStringList documentExtensions;
    QString bundlePath;
    QString executablePath;
    QString iconPath;
    BundleProvenance provenance;
};

inline bool operator==(const BundleApplication &left, const BundleApplication &right)
{
    return left.bundleId == right.bundleId
        && left.desktopId == right.desktopId
        && left.name == right.name
        && left.version == right.version
        && left.executable == right.executable
        && left.icon == right.icon
        && left.categories == right.categories
        && left.documentExtensions == right.documentExtensions
        && left.bundlePath == right.bundlePath
        && left.executablePath == right.executablePath
        && left.iconPath == right.iconPath
        && left.provenance == right.provenance;
}

class ApplicationBundleCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList applications READ applications NOTIFY applicationsChanged)

public:
    explicit ApplicationBundleCatalog(QStringList bundleDirectories = {}, QObject *parent = nullptr);

    QList<BundleApplication> entries() const;
    QVariantList applications() const;
    QVariantList searchApplications(const QString &query) const;
    QStringList applicationIds() const;

    bool reload();
    bool launchSpec(const QString &desktopId, QString *program, QStringList *arguments) const;

    static bool inspectBundle(const QString &path, BundleApplication *application);
    static QStringList defaultBundleDirectories();

signals:
    void applicationsChanged();

private:
    static QVariantList toVariantList(const QList<BundleApplication> &entries);
    void scheduleReload();
    void refreshWatchPaths();

    QStringList m_bundleDirectories;
    QList<BundleApplication> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
};
