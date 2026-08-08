#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "../launcher/applicationcatalog.h"
#include "../launcher/applicationbundlecatalog.h"

class ApplicationLauncher final : public QObject
{
    Q_OBJECT

public:
    using LaunchFunction = std::function<bool(const QString &program,
                                              const QStringList &arguments,
                                              qint64 *pid)>;

    explicit ApplicationLauncher(
        QObject *parent = nullptr,
        LaunchFunction launchFunction = {},
        QStringList applicationDirectories = {},
        QString launchLogPath = {},
        QStringList bundleDirectories = {},
        QString associationSettingsPath = {});

    Q_PROPERTY(QVariantList applications READ applications NOTIFY applicationsChanged)
    Q_PROPERTY(QString applicationQuery READ applicationQuery WRITE setApplicationQuery NOTIFY applicationQueryChanged)
    Q_PROPERTY(QVariantList matchingApplications READ matchingApplications NOTIFY matchingApplicationsChanged)
    Q_PROPERTY(QString launchMessage READ launchMessage NOTIFY launchStatusChanged)
    Q_PROPERTY(QString lastLaunchDesktopId READ lastLaunchDesktopId NOTIFY launchStatusChanged)
    Q_PROPERTY(QString lastLaunchProgram READ lastLaunchProgram NOTIFY launchStatusChanged)
    Q_PROPERTY(qint64 lastLaunchPid READ lastLaunchPid NOTIFY launchStatusChanged)
    Q_PROPERTY(bool lastLaunchSucceeded READ lastLaunchSucceeded NOTIFY launchStatusChanged)
    Q_PROPERTY(QString launchLogPath READ launchLogPath CONSTANT)

    QVariantList applications() const;
    QString applicationQuery() const;
    QVariantList matchingApplications() const;
    QString launchMessage() const;
    QString lastLaunchDesktopId() const;
    QString lastLaunchProgram() const;
    qint64 lastLaunchPid() const;
    bool lastLaunchSucceeded() const;
    QString launchLogPath() const;
    Q_INVOKABLE bool launchTerminal();
    Q_INVOKABLE bool launchBrowser();
    Q_INVOKABLE bool launchApplication(const QString &desktopId);
    Q_INVOKABLE bool launchApplicationWithFile(const QString &desktopId, const QString &filePath);
    Q_INVOKABLE QString preferredApplicationForFile(const QString &filePath) const;
    Q_INVOKABLE bool setPreferredApplicationForFile(const QString &filePath, const QString &desktopId);
    Q_INVOKABLE bool clearPreferredApplicationForFile(const QString &filePath);
    Q_INVOKABLE bool refreshApplications();
    Q_INVOKABLE void clearLaunchMessage();

public slots:
    void setApplicationQuery(const QString &query);

signals:
    void applicationsChanged();
    void fileAssociationsChanged();
    void applicationQueryChanged();
    void matchingApplicationsChanged();
    void launchStatusChanged();

private:
    bool launch(const QString &desktopId,
                const QString &applicationName,
                const QString &program,
                const QStringList &arguments);
    void recordLaunch(const QString &desktopId,
                      const QString &applicationName,
                      const QString &program,
                      qint64 pid,
                      bool succeeded);
    void setLaunchStatus(const QString &desktopId,
                         const QString &applicationName,
                         const QString &program,
                         qint64 pid,
                         bool succeeded);
    QString applicationNameFor(const QString &desktopId) const;
    bool launchSpec(const QString &desktopId, QString *program, QStringList *arguments) const;
    QStringList applicationIds() const;
    static QString associationExtension(const QString &filePath);
    bool ensureAssociationSettingsDirectory() const;

    ApplicationCatalog m_catalog;
    ApplicationBundleCatalog m_bundleCatalog;
    QString m_applicationQuery;
    LaunchFunction m_launchFunction;
    QString m_launchLogPath;
    QString m_associationSettingsPath;
    QString m_launchMessage;
    QString m_lastLaunchDesktopId;
    QString m_lastLaunchProgram;
    qint64 m_lastLaunchPid = 0;
    bool m_lastLaunchSucceeded = false;
};
