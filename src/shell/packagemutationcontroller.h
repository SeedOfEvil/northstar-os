#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class PackageCatalog;
class QProcess;

class PackageMutationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authorizationAvailable READ authorizationAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool planReady READ planReady NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY stateChanged)
    Q_PROPERTY(QString operation READ operation NOTIFY stateChanged)
    Q_PROPERTY(QString packageName READ packageName NOTIFY stateChanged)

public:
    explicit PackageMutationController(PackageCatalog *catalog = nullptr,
                                       QString packageManagerPath = {},
                                       QString transactionPath = {},
                                       QString authorizationPath = {},
                                       QObject *parent = nullptr);

    bool authorizationAvailable() const;
    bool busy() const;
    bool planReady() const;
    QString status() const;
    QString preview() const;
    QString operation() const;
    QString packageName() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool planInstall(const QVariantMap &package);
    Q_INVOKABLE bool planRemove(const QVariantMap &package);
    Q_INVOKABLE bool applyPlan();
    Q_INVOKABLE void clearPlan();

    static QByteArray recordPayload(qint64 timestamp,
                                    const QString &operation,
                                    const QString &repository,
                                    const QString &catalogueDigest,
                                    int index,
                                    const QString &name,
                                    const QString &version,
                                    const QString &origin);
    static QString planIdentifier(qint64 timestamp,
                                  int index,
                                  const QByteArray &recordHash,
                                  const QByteArray &previewHash);

signals:
    void stateChanged();
    void transactionFinished(bool success);

private:
    bool beginPlan(const QString &operation, const QVariantMap &package);
    void finishPreview(int exitCode, int exitStatus);
    void setFailure(const QString &message);

    PackageCatalog *m_catalog = nullptr;
    QString m_packageManagerPath;
    QString m_transactionPath;
    QString m_authorizationPath;
    QProcess *m_previewProcess = nullptr;
    QProcess *m_transactionProcess = nullptr;
    QVariantMap m_selectedPackage;
    QString m_status;
    QString m_preview;
    QString m_operation;
    QString m_planIdentifier;
    bool m_authorizationAvailable = false;
    bool m_busy = false;
    bool m_planReady = false;
};
