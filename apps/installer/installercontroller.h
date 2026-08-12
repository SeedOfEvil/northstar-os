#pragma once

#include "installerdiskmodel.h"

#include <QObject>

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

public:
    explicit InstallerController(QObject *parent = nullptr, QString discoveryCommand = {});
    InstallerDiskModel *disks();
    bool busy() const;
    QString statusMessage() const;
    int selectedIndex() const;
    QString selectedDevice() const;
    bool confirmationReady() const;
    bool planReady() const;
    QString planSummary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool selectDisk(int index);
    Q_INVOKABLE void setConfirmationText(const QString &text);
    Q_INVOKABLE void setEraseAcknowledged(bool acknowledged);
    Q_INVOKABLE bool preparePlan();
    Q_INVOKABLE void resetPlan();

signals:
    void stateChanged();

private:
    bool parseDiscovery(const QByteArray &output, QString *error);
    void resetSelection();

    InstallerDiskModel m_disks;
    QProcess *m_process = nullptr;
    QString m_discoveryCommand;
    QString m_statusMessage;
    QString m_confirmationText;
    QString m_planSummary;
    int m_selectedIndex = -1;
    bool m_busy = false;
    bool m_eraseAcknowledged = false;
    bool m_planReady = false;
};
