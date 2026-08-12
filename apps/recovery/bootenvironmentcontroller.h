#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QProcess;

class BootEnvironmentController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QVariantList environments READ environments NOTIFY stateChanged)
    Q_PROPERTY(QString selectedEnvironment READ selectedEnvironment NOTIFY stateChanged)
    Q_PROPERTY(QString confirmationText READ confirmationText NOTIFY stateChanged)
    Q_PROPERTY(bool activationReady READ activationReady NOTIFY stateChanged)
    Q_PROPERTY(bool rebootRequired READ rebootRequired NOTIFY stateChanged)
    Q_PROPERTY(QString diagnosticPath READ diagnosticPath NOTIFY stateChanged)

public:
    explicit BootEnvironmentController(QObject *parent = nullptr,
                                       QString recoveryCommand = {},
                                       QString diagnosticDirectory = {});

    bool busy() const;
    QString state() const;
    QString statusMessage() const;
    QVariantList environments() const;
    QString selectedEnvironment() const;
    QString confirmationText() const;
    bool activationReady() const;
    bool rebootRequired() const;
    QString diagnosticPath() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectEnvironment(const QString &name);
    Q_INVOKABLE void setConfirmationText(const QString &text);
    Q_INVOKABLE void scheduleActivation();
    Q_INVOKABLE bool exportDiagnostics();

signals:
    void stateChanged();

private:
    enum class Operation { None, Status, Activate };

    void start(Operation operation);
    void handleFinished(int exitCode, int exitStatus);
    bool parseStatus(const QByteArray &output, QString *errorMessage);
    bool parseActivation(const QByteArray &output, QString *errorMessage);
    void failOperation(const QString &message);

    QProcess *m_process = nullptr;
    QString m_recoveryCommand;
    QString m_diagnosticDirectory;
    QString m_state = QStringLiteral("unchecked");
    QString m_statusMessage = QStringLiteral("Refresh to inspect available boot environments.");
    QVariantList m_environments;
    QString m_selectedEnvironment;
    QString m_confirmationText;
    QString m_diagnosticPath;
    Operation m_operation = Operation::None;
    bool m_busy = false;
    bool m_rebootRequired = false;
};
