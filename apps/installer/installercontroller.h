#pragma once

#include "installerdiskmodel.h"

#include <QObject>
#include <QTemporaryFile>
#include <memory>

class QProcess;

class InstallerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(InstallerDiskModel *disks READ disks CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY stateChanged)
    Q_PROPERTY(QString selectedDevice READ selectedDevice NOTIFY stateChanged)
    Q_PROPERTY(bool confirmationReady READ confirmationReady NOTIFY stateChanged)
    Q_PROPERTY(bool planReady READ planReady NOTIFY stateChanged)
    Q_PROPERTY(QString planSummary READ planSummary NOTIFY stateChanged)
    Q_PROPERTY(QString installationState READ installationState NOTIFY stateChanged)
    Q_PROPERTY(bool installationActive READ installationActive NOTIFY stateChanged)
    Q_PROPERTY(bool installationComplete READ installationComplete NOTIFY stateChanged)
    Q_PROPERTY(QString transactionId READ transactionId NOTIFY stateChanged)

public:
    explicit InstallerController(QObject *parent = nullptr, QString discoveryCommand = {},
                                 QString stageCommand = {}, QString executeCommand = {},
                                 QString manifestPath = {});
    InstallerDiskModel *disks();
    bool busy() const;
    QString statusMessage() const;
    int selectedIndex() const;
    QString selectedDevice() const;
    bool confirmationReady() const;
    bool planReady() const;
    QString planSummary() const;
    QString installationState() const;
    bool installationActive() const;
    bool installationComplete() const;
    QString transactionId() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool selectDisk(int index);
    Q_INVOKABLE void setConfirmationText(const QString &text);
    Q_INVOKABLE void setEraseAcknowledged(bool acknowledged);
    Q_INVOKABLE bool preparePlan();
    Q_INVOKABLE bool beginInstallation();
    Q_INVOKABLE void resetPlan();

signals:
    void stateChanged();

private:
    bool parseDiscovery(const QByteArray &output, QString *error);
    bool createRequest(QString *error);
    bool parseStageResult(const QByteArray &output, QString *error);
    bool parseExecutionResult(const QByteArray &output, QString *error);
    void startProtected(const QString &command, const QString &fixedProgram,
                        const QStringList &arguments);
    void failInstallation(const QString &message);
    void resetSelection();

    enum class Operation { None, Discovery, Stage, Execute };

    InstallerDiskModel m_disks;
    QProcess *m_process = nullptr;
    QString m_discoveryCommand;
    QString m_stageCommand;
    QString m_executeCommand;
    QString m_manifestPath;
    QString m_statusMessage;
    QString m_confirmationText;
    QString m_planSummary;
    QString m_installationState = QStringLiteral("idle");
    QString m_transactionId;
    std::unique_ptr<QTemporaryFile> m_request;
    Operation m_operation = Operation::None;
    int m_selectedIndex = -1;
    bool m_busy = false;
    bool m_eraseAcknowledged = false;
    bool m_planReady = false;
};
