#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

struct InstalledPackage
{
    QString name;
    QString version;
    QString comment;
};

class PackageCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
    Q_PROPERTY(QVariantList matchingPackages READ matchingPackages NOTIFY matchingPackagesChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString packageManagerPath READ packageManagerPath CONSTANT)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY packagesChanged)
    Q_PROPERTY(QString lastRefresh READ lastRefresh NOTIFY statusChanged)

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
    QString lastRefresh() const;

    Q_INVOKABLE bool refresh();

    static QList<InstalledPackage> parseQueryOutput(const QByteArray &output);
    static QList<InstalledPackage> filterPackages(const QList<InstalledPackage> &packages,
                                                  const QString &query);

public slots:
    void setQuery(const QString &query);

signals:
    void packagesChanged();
    void matchingPackagesChanged();
    void queryChanged();
    void statusChanged();

private:
    static QVariantList toVariantList(const QList<InstalledPackage> &packages);
    void setStatusMessage(const QString &message);

    QString m_packageManagerPath;
    QList<InstalledPackage> m_packages;
    QString m_query;
    QString m_statusMessage;
    QString m_lastRefresh;
    bool m_refreshing = false;
};
