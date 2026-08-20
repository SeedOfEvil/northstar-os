#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QProcess>
#include <QStringList>
#include <QVariantList>

struct InstalledPackage
{
    QString name;
    QString version;
    QString comment;

    // pkg records whether a package was asked for or arrived as a dependency
    // of something else. On this validation machine 445 of 481 installed
    // packages are automatic, so a list that does not make the distinction is
    // almost entirely things nobody chose.
    bool automatic = false;

    // Filled in by the update scan, which runs separately because it is two
    // orders of magnitude slower than reading the inventory.
    bool updatable = false;
    QString availableVersion;

    // An installed package whose origin has left the ports tree. It cannot be
    // updated, and saying so is more use than showing it as current.
    bool orphaned = false;
};

class PackageCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
    Q_PROPERTY(QVariantList matchingPackages READ matchingPackages NOTIFY matchingPackagesChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString packageManagerPath READ packageManagerPath CONSTANT)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY packagesChanged)
    Q_PROPERTY(int requestedCount READ requestedCount NOTIFY packagesChanged)
    Q_PROPERTY(int dependencyCount READ dependencyCount NOTIFY packagesChanged)
    Q_PROPERTY(int updatableCount READ updatableCount NOTIFY packagesChanged)
    Q_PROPERTY(QString lastRefresh READ lastRefresh NOTIFY statusChanged)

    // Which packages the surface is listing. Reading every installed package
    // is still possible, but it is not what the window opens on.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

    // The update scan takes seconds because it consults the repository
    // catalogue, so it runs on its own and the surface says while it is
    // running rather than freezing until it finishes.
    Q_PROPERTY(bool scanningUpdates READ scanningUpdates NOTIFY updateScanChanged)
    Q_PROPERTY(bool updatesKnown READ updatesKnown NOTIFY updateScanChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY updateScanChanged)

public:
    explicit PackageCatalog(QString packageManagerPath = {}, QObject *parent = nullptr);

    QVariantList packages() const;
    QVariantList matchingPackages() const;
    QString query() const;
    bool available() const;
    bool refreshing() const;
    QString statusMessage() const;
    QString packageManagerPath() const;
    int installedCount() const;
    int requestedCount() const;
    int dependencyCount() const;
    int updatableCount() const;
    QString lastRefresh() const;
    QString filter() const;
    bool scanningUpdates() const;
    bool updatesKnown() const;
    QString updateStatus() const;

    // The filters the surface may ask for. Anything else is refused rather
    // than silently listing everything.
    static QStringList filters();
    static QString requestedFilter();
    static QString updatableFilter();
    static QString allFilter();

    Q_INVOKABLE bool refresh();

    // Starts the update scan. Returns whether it started; the result arrives
    // later through updateScanChanged.
    Q_INVOKABLE bool scanForUpdates();

    static QList<InstalledPackage> parseVersionOutput(const QByteArray &output);

    static QList<InstalledPackage> parseQueryOutput(const QByteArray &output);
    static QList<InstalledPackage> filterPackages(const QList<InstalledPackage> &packages,
                                                  const QString &query);

public slots:
    void setQuery(const QString &query);
    void setFilter(const QString &filter);

signals:
    void packagesChanged();
    void matchingPackagesChanged();
    void queryChanged();
    void statusChanged();
    void refreshingChanged();
    void filterChanged();
    void updateScanChanged();

private:
    static QVariantList toVariantList(const QList<InstalledPackage> &packages);
    QList<InstalledPackage> visiblePackages() const;
    void applyUpdateScan(const QByteArray &output);
    void setStatusMessage(const QString &message);

    QString m_packageManagerPath;
    QList<InstalledPackage> m_packages;
    QString m_query;
    QString m_filter;
    QString m_statusMessage;
    QString m_lastRefresh;
    QString m_updateStatus;
    bool m_refreshing = false;
    bool m_updatesKnown = false;
    QProcess *m_updateScan = nullptr;
};
