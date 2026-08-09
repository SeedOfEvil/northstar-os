#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

class PackageTrustController;

struct RepositoryPackageMetadata
{
    QString name;
    QString version;
    QString origin;
    QString source;
    QString projectRevision;
};

struct RepositoryMetadata
{
    int schemaVersion = 0;
    QString repositoryTag;
    QString channel;
    QString abi;
    int revision = 0;
    QString generatedAt;
    QString sourceRevision;
    QString signatureStatus;
    QString signatureFingerprint;
    QString signatureEnvelope;
    QString catalogueFile;
    QString catalogueSha256;
    QList<RepositoryPackageMetadata> packages;
};

class UpdatePlanController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool metadataPresent READ metadataPresent NOTIFY stateChanged)
    Q_PROPERTY(bool metadataValid READ metadataValid NOTIFY stateChanged)
    Q_PROPERTY(bool cataloguePresent READ cataloguePresent NOTIFY stateChanged)
    Q_PROPERTY(bool catalogueDigestValid READ catalogueDigestValid NOTIFY stateChanged)
    Q_PROPERTY(bool signatureVerified READ signatureVerified NOTIFY stateChanged)
    Q_PROPERTY(QString signatureStatus READ signatureStatus NOTIFY stateChanged)
    Q_PROPERTY(int packageCount READ packageCount NOTIFY stateChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY stateChanged)
    Q_PROPERTY(int installCount READ installCount NOTIFY stateChanged)
    Q_PROPERTY(int unmanagedCount READ unmanagedCount NOTIFY stateChanged)
    Q_PROPERTY(QString metadataPath READ metadataPath CONSTANT)
    Q_PROPERTY(QString catalogueFile READ catalogueFile NOTIFY stateChanged)
    Q_PROPERTY(QString catalogueStatus READ catalogueStatus NOTIFY stateChanged)
    Q_PROPERTY(QString metadataStatus READ metadataStatus NOTIFY stateChanged)
    Q_PROPERTY(QString planStatus READ planStatus NOTIFY stateChanged)
    Q_PROPERTY(QString planPreview READ planPreview NOTIFY stateChanged)

public:
    explicit UpdatePlanController(PackageTrustController *trustController = nullptr,
                                  QString metadataPath = {},
                                  QObject *parent = nullptr);

    bool metadataPresent() const;
    bool metadataValid() const;
    bool cataloguePresent() const;
    bool catalogueDigestValid() const;
    bool signatureVerified() const;
    QString signatureStatus() const;
    int packageCount() const;
    int updateCount() const;
    int installCount() const;
    int unmanagedCount() const;
    QString metadataPath() const;
    QString catalogueFile() const;
    QString catalogueStatus() const;
    QString metadataStatus() const;
    QString planStatus() const;
    QString planPreview() const;

    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool preview(const QVariantList &installedPackages);

    static bool parseMetadata(const QByteArray &contents,
                              RepositoryMetadata &metadata,
                              QString *errorMessage = nullptr);

signals:
    void stateChanged();

private:
    static QString defaultMetadataPath();
    bool verifyCatalogueIntegrity(QString *errorMessage = nullptr);
    bool verifySignature(QString *errorMessage = nullptr);
    void resetPlan();
    void setBlockedPlan(const QString &reason);

    PackageTrustController *m_trustController = nullptr;
    QString m_metadataPath;
    RepositoryMetadata m_metadata;
    QString m_signatureStatus;
    QString m_metadataStatus;
    QString m_planStatus;
    QString m_planPreview;
    bool m_metadataPresent = false;
    bool m_metadataValid = false;
    bool m_cataloguePresent = false;
    bool m_catalogueDigestValid = false;
    bool m_signatureVerified = false;
    QString m_catalogueFile;
    QString m_catalogueStatus;
    int m_updateCount = 0;
    int m_installCount = 0;
    int m_unmanagedCount = 0;
};
