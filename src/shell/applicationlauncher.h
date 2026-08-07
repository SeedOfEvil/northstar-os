#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "../launcher/applicationcatalog.h"

class ApplicationLauncher final : public QObject
{
    Q_OBJECT

public:
    using LaunchFunction = std::function<bool(const QString &program, const QStringList &arguments)>;

    explicit ApplicationLauncher(
        QObject *parent = nullptr,
        LaunchFunction launchFunction = {},
        QStringList applicationDirectories = {});

    Q_PROPERTY(QVariantList applications READ applications NOTIFY applicationsChanged)
    Q_PROPERTY(QString applicationQuery READ applicationQuery WRITE setApplicationQuery NOTIFY applicationQueryChanged)
    Q_PROPERTY(QVariantList matchingApplications READ matchingApplications NOTIFY matchingApplicationsChanged)

    QVariantList applications() const;
    QString applicationQuery() const;
    QVariantList matchingApplications() const;
    Q_INVOKABLE bool launchTerminal() const;
    Q_INVOKABLE bool launchBrowser() const;
    Q_INVOKABLE bool launchApplication(const QString &desktopId) const;
    Q_INVOKABLE bool refreshApplications();

public slots:
    void setApplicationQuery(const QString &query);

signals:
    void applicationsChanged();
    void applicationQueryChanged();
    void matchingApplicationsChanged();

private:
    bool launch(const QString &program, const QStringList &arguments) const;

    ApplicationCatalog m_catalog;
    QString m_applicationQuery;
    LaunchFunction m_launchFunction;
};
