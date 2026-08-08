#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

struct PackageRepositoryPolicy
{
    QString channel;
    QString repositoryName;
    QString repositoryUrl;
    QString signingKeyFingerprint;
    QString trustMode;
};

class PackageTrustController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool policyPresent READ policyPresent NOTIFY stateChanged)
    Q_PROPERTY(bool policyValid READ policyValid NOTIFY stateChanged)
    Q_PROPERTY(QString channel READ channel NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryName READ repositoryName NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryUrl READ repositoryUrl NOTIFY stateChanged)
    Q_PROPERTY(QString signingKeyFingerprint READ signingKeyFingerprint NOTIFY stateChanged)
    Q_PROPERTY(QString policyPath READ policyPath CONSTANT)
    Q_PROPERTY(QString trustStatus READ trustStatus NOTIFY stateChanged)
    Q_PROPERTY(QString updatePlanStatus READ updatePlanStatus NOTIFY stateChanged)

public:
    explicit PackageTrustController(QString policyPath = {}, QObject *parent = nullptr);

    bool policyPresent() const;
    bool policyValid() const;
    QString channel() const;
    QString repositoryName() const;
    QString repositoryUrl() const;
    QString signingKeyFingerprint() const;
    QString policyPath() const;
    QString trustStatus() const;
    QString updatePlanStatus() const;

    Q_INVOKABLE bool reload();
    Q_INVOKABLE void planUpdate();

    static bool parsePolicy(const QByteArray &contents,
                            PackageRepositoryPolicy &policy,
                            QString *errorMessage = nullptr);

signals:
    void stateChanged();

private:
    static QString defaultPolicyPath();
    void setFailure(const QString &trustStatus, const QString &planStatus);

    QString m_policyPath;
    PackageRepositoryPolicy m_policy;
    QString m_trustStatus;
    QString m_updatePlanStatus;
    bool m_policyPresent = false;
    bool m_policyValid = false;
};
