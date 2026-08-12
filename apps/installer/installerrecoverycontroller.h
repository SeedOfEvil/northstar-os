#pragma once

#include <QObject>
#include <QString>

class QProcess;

class InstallerRecoveryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QString transactionId READ transactionId NOTIFY stateChanged)
    Q_PROPERTY(QString targetDevice READ targetDevice NOTIFY stateChanged)
    Q_PROPERTY(QString lastPhase READ lastPhase NOTIFY stateChanged)
    Q_PROPERTY(bool mutationStarted READ mutationStarted NOTIFY stateChanged)
    Q_PROPERTY(QString recoveryAction READ recoveryAction NOTIFY stateChanged)
    Q_PROPERTY(bool interruptedExecution READ interruptedExecution NOTIFY stateChanged)
    Q_PROPERTY(bool retryConfirmationReady READ retryConfirmationReady NOTIFY stateChanged)
    Q_PROPERTY(bool diagnosticsReady READ diagnosticsReady NOTIFY stateChanged)
    Q_PROPERTY(QString diagnosticPreview READ diagnosticPreview NOTIFY stateChanged)
    Q_PROPERTY(QString diagnosticPath READ diagnosticPath NOTIFY stateChanged)

public:
    explicit InstallerRecoveryController(QObject *parent = nullptr,
                                         QString recoveryCommand = {},
                                         QString diagnosticDirectory = {});

    bool busy() const;
    QString state() const;
    QString statusMessage() const;
    QString transactionId() const;
    QString targetDevice() const;
    QString lastPhase() const;
    bool mutationStarted() const;
    QString recoveryAction() const;
    bool interruptedExecution() const;
    bool retryConfirmationReady() const;
    bool diagnosticsReady() const;
    QString diagnosticPreview() const;
    QString diagnosticPath() const;

    Q_INVOKABLE void checkStatus();
    Q_INVOKABLE void setRetryConfirmationText(const QString &text);
    Q_INVOKABLE void exportDiagnostics();
    Q_INVOKABLE void prepareCleanRetry();
    Q_INVOKABLE void reset();

signals:
    void stateChanged();
    void retryPrepared();

private:
    enum class Operation { None, Status, Diagnostics, PrepareRetry };

    void start(Operation operation);
    void handleFinished(int exitCode, int exitStatus);
    bool parseStatus(const QByteArray &output, QString *errorMessage);
    bool parseDiagnostics(const QByteArray &output, QString *errorMessage);
    bool parseRetry(const QByteArray &output, QString *errorMessage);
    void clearTransaction();
    void failOperation(const QString &message);

    QProcess *m_process = nullptr;
    QString m_recoveryCommand;
    QString m_diagnosticDirectory;
    QString m_state = QStringLiteral("unchecked");
    QString m_statusMessage = QStringLiteral("Check for a previous interrupted installation.");
    QString m_transactionId;
    QString m_targetDevice;
    QString m_lastPhase;
    QString m_recoveryAction;
    QString m_retryConfirmationText;
    QString m_diagnosticPreview;
    QString m_diagnosticPath;
    Operation m_operation = Operation::None;
    bool m_busy = false;
    bool m_mutationStarted = false;
    bool m_diagnosticsReady = false;
};
