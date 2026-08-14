#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;
class QTemporaryFile;

class FirstBootController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QStringList locales READ locales CONSTANT)
    Q_PROPERTY(QStringList timezones READ timezones CONSTANT)
    Q_PROPERTY(QString defaultTimezone READ defaultTimezone CONSTANT)
    Q_PROPERTY(QStringList keyboardLayouts READ keyboardLayouts CONSTANT)

public:
    explicit FirstBootController(QObject *parent = nullptr);

    bool busy() const;
    QString statusMessage() const;
    QStringList locales() const;
    QStringList timezones() const;
    QString defaultTimezone() const;
    QStringList keyboardLayouts() const;

    Q_INVOKABLE QString validateProfile(const QString &displayName,
                                        const QString &username,
                                        const QString &password,
                                        const QString &passwordConfirmation,
                                        const QString &locale,
                                        const QString &timezone,
                                        const QString &keyboardLayout) const;
    Q_INVOKABLE bool provision(const QString &displayName,
                               const QString &username,
                               const QString &password,
                               const QString &passwordConfirmation,
                               const QString &locale,
                               const QString &timezone,
                               const QString &keyboardLayout);

signals:
    void stateChanged();
    void secretsCleared();
    void provisioningFinished(bool success);

private:
    static bool containsControlCharacters(const QString &value);
    void finishProvisioning(bool success, const QString &message);

    QProcess *m_process = nullptr;
    QTemporaryFile *m_request = nullptr;
    QByteArray m_pendingPassword;
    bool m_busy = false;
    QString m_statusMessage;
};
