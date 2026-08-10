#pragma once

#include <QObject>
#include <QString>

class PackageTrustController;
class UpdatePlanController;

class UpdateAuthorizationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool preflightValid READ preflightValid NOTIFY stateChanged)
    Q_PROPERTY(bool bectlAvailable READ bectlAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool zfsAvailable READ zfsAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool authorizationAvailable READ authorizationAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString bootEnvironmentName READ bootEnvironmentName NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString plan READ plan NOTIFY stateChanged)

public:
    explicit UpdateAuthorizationController(PackageTrustController *trustController = nullptr,
                                            UpdatePlanController *updatePlan = nullptr,
                                            QString bectlPath = {},
                                            QString zfsPath = {},
                                            QObject *parent = nullptr);

    bool preflightValid() const;
    bool bectlAvailable() const;
    bool zfsAvailable() const;
    bool authorizationAvailable() const;
    QString bootEnvironmentName() const;
    QString status() const;
    QString plan() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool applyUpdate();
    Q_INVOKABLE bool scheduleRollback();

signals:
    void stateChanged();

private:
    static bool executableAvailable(const QString &overridePath, const QString &name);
    static QString makeBootEnvironmentName(const UpdatePlanController &updatePlan,
                                           const QString &channel);
    bool requestTransaction(const QString &operation);
    void setState(bool preflightValid,
                  bool bectlAvailable,
                  bool zfsAvailable,
                  const QString &status,
                  const QString &plan,
                  const QString &bootEnvironmentName = {});

    PackageTrustController *m_trustController = nullptr;
    UpdatePlanController *m_updatePlan = nullptr;
    QString m_bectlPath;
    QString m_zfsPath;
    bool m_preflightValid = false;
    bool m_bectlAvailable = false;
    bool m_zfsAvailable = false;
    bool m_authorizationAvailable = false;
    QString m_bootEnvironmentName;
    QString m_status;
    QString m_plan;
};
