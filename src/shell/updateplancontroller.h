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
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY stateChanged)
    Q_PROPERTY(QString signatureStatus READ signatureStatus NOTIFY stateChanged)
    Q_PROPERTY(QString signatureFingerprint READ signatureFingerprint NOTIFY stateChanged)
    Q_PROPERTY(QString catalogueSha256 READ catalogueSha256 NOTIFY stateChanged)
    Q_PROPERTY(QString publicationManifestSha256 READ publicationManifestSha256 NOTIFY stateChanged)
    Q_PROPERTY(QString repositoryTag READ repositoryTag NOTIFY stateChanged)
    Q_PROPERTY(QString channel READ channel NOTIFY stateChanged)
    Q_PROPERTY(QString abi READ abi NOTIFY stateChanged)
    Q_PROPERTY(QString generatedAt READ generatedAt NOTIFY stateChanged)
    Q_PROPERTY(QVariantList packageProvenance READ packageProvenance NOTIFY stateChanged)
    // The revision the trust material installed on this system describes.
    // Not necessarily the one the repository is publishing.
    Q_PROPERTY(int repositoryRevision READ repositoryRevision NOTIFY stateChanged)

    // The revision the active repository actually publishes, read from its
    // own publication record, or 0 when that cannot be read. The two
    // disagreeing is a state a person can act on, and was previously
    // reported only as an unverified signature.
    Q_PROPERTY(int publishedRevision READ publishedRevision NOTIFY stateChanged)

    // Why planning is blocked, said in terms of what is out of step rather
    // than which check failed.
    Q_PROPERTY(QString blockedReason READ blockedReason NOTIFY stateChanged)
    Q_PROPERTY(QString sourceRevision READ sourceRevision NOTIFY stateChanged)
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
    // repositoryConfigDirectory names where the active pkg repository
    // configuration lives. It is a parameter so a test can point at one it
    // wrote, rather than at whatever this machine happens to have.
    explicit UpdatePlanController(PackageTrustController *trustController = nullptr,
                                  QString metadataPath = {},
                                  QObject *parent = nullptr,
                                  QString repositoryConfigDirectory = {});

    bool metadataPresent() const;
    bool metadataValid() const;
    bool cataloguePresent() const;
    bool catalogueDigestValid() const;
    bool signatureVerified() const;
    bool previewReady() const;
    QString signatureStatus() const;
    QString signatureFingerprint() const;
    QString catalogueSha256() const;
    QString publicationManifestSha256() const;
    QString repositoryTag() const;
    QString channel() const;
    QString abi() const;
    QString generatedAt() const;
    QVariantList packageProvenance() const;
    int repositoryRevision() const;
    QString sourceRevision() const;
    int packageCount() const;
    int updateCount() const;
    int installCount() const;
    int unmanagedCount() const;
    QString metadataPath() const;
    int publishedRevision() const;
    QString blockedReason() const;

    // Where the active pkg configuration says the repository is. Empty when
    // it names something other than a local path, which is the only kind
    // this can read without fetching anything.
    static QString activeRepositoryPath(const QString &repositoryConfigDirectory = {});
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
    int readPublishedRevision() const;
    bool verifyCatalogueIntegrity(QString *errorMessage = nullptr);
    bool verifySignature(QString *errorMessage = nullptr);
    void resetPlan();
    void setBlockedPlan(const QString &reason);

    PackageTrustController *m_trustController = nullptr;
    QString m_metadataPath;
    QString m_repositoryConfigDirectory;

    int m_publishedRevision = 0;
    QString m_blockedReason;
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
    bool m_previewReady = false;
    QString m_catalogueFile;
    QString m_catalogueStatus;
    QString m_publicationManifestSha256;
    int m_updateCount = 0;
    int m_installCount = 0;
    int m_unmanagedCount = 0;
};
