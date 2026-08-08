#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>

struct BundleApplication
{
    QString bundleId;
    QString desktopId;
    QString name;
    QString version;
    QString executable;
    QString icon;
    QStringList categories;
    QString bundlePath;
    QString executablePath;
    QString iconPath;
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
        && left.bundlePath == right.bundlePath
        && left.executablePath == right.executablePath
        && left.iconPath == right.iconPath;
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

    static QStringList defaultBundleDirectories();

signals:
    void applicationsChanged();

private:
    static QVariantList toVariantList(const QList<BundleApplication> &entries);

    QStringList m_bundleDirectories;
    QList<BundleApplication> m_entries;
};
