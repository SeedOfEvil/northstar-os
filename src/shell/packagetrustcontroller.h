#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

struct PackageRepositoryPolicy
{
    QString channel;
    QString repositoryTag;
    QString repositoryName;
    QString repositoryUrl;
    QString mirrorType;
    QString signatureType;
    QString fingerprintsPath;
    QString trustMode;
};

class PackageTrustController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool policyPresent READ policyPresent NOTIFY stateChanged)
    Q_PROPERTY(bool policyValid READ policyValid NOTIFY stateChanged)
    Q_PROPERTY(bool trustStoreValid READ trustStoreValid NOTIFY stateChanged)
    Q_PROPERTY(int trustedFingerprintCount READ trustedFingerprintCount NOTIFY stateChanged)
    Q_PROPERTY(int revokedFingerprintCount READ revokedFingerprintCount NOTIFY stateChanged)
    Q_PROPERTY(QString channel READ channel NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryTag READ repositoryTag NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryName READ repositoryName NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryUrl READ repositoryUrl NOTIFY stateChanged)
    Q_PROPERTY(QString mirrorType READ mirrorType NOTIFY stateChanged)
    Q_PROPERTY(QString signatureType READ signatureType NOTIFY stateChanged)
    Q_PROPERTY(QString fingerprintsPath READ fingerprintsPath NOTIFY stateChanged)
    Q_PROPERTY(QString policyPath READ policyPath CONSTANT)
    Q_PROPERTY(QString repositoryConfigPreview READ repositoryConfigPreview NOTIFY stateChanged)
    Q_PROPERTY(QString trustStatus READ trustStatus NOTIFY stateChanged)
    Q_PROPERTY(QString updatePlanStatus READ updatePlanStatus NOTIFY stateChanged)

public:
    explicit PackageTrustController(QString policyPath = {}, QObject *parent = nullptr);

    bool policyPresent() const;
    bool policyValid() const;
    bool trustStoreValid() const;
    int trustedFingerprintCount() const;
    int revokedFingerprintCount() const;
    QString channel() const;
    QString repositoryTag() const;
    QString repositoryName() const;
    QString repositoryUrl() const;
    QString mirrorType() const;
    QString signatureType() const;
    QString fingerprintsPath() const;
    QString policyPath() const;
    QString repositoryConfigPreview() const;
    QString trustStatus() const;
    QString updatePlanStatus() const;

    Q_INVOKABLE bool reload();
    Q_INVOKABLE void planUpdate();

    static bool parsePolicy(const QByteArray &contents,
                            PackageRepositoryPolicy &policy,
                            QString *errorMessage = nullptr);
    static QString renderPkgRepositoryConfig(const PackageRepositoryPolicy &policy);
    static bool parseFingerprintFile(const QByteArray &contents,
                                     QString &fingerprint,
                                     QString *errorMessage = nullptr);
    static bool loadFingerprintStore(const QString &fingerprintsPath,
                                     int &trustedCount,
                                     int &revokedCount,
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
    QString m_repositoryConfigPreview;
    bool m_policyPresent = false;
    bool m_policyValid = false;
    bool m_trustStoreValid = false;
    int m_trustedFingerprintCount = 0;
    int m_revokedFingerprintCount = 0;
};
