#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Reads and changes the system clock's configuration: which timezone the
// machine is in, and whether it keeps itself set from the network.
//
// Every change goes through the northstar-clock boundary, which is installed
// separately from the shell. Where that boundary is absent the controller
// still reports what it can read and declares the controls read-only, rather
// than offering a switch with nothing behind it.
class ClockController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString timeZone READ timeZone NOTIFY clockChanged)
    Q_PROPERTY(QString timeZoneStatus READ timeZoneStatus NOTIFY clockChanged)
    Q_PROPERTY(bool timeZoneKnown READ timeZoneKnown NOTIFY clockChanged)
    Q_PROPERTY(bool timeZoneWritable READ timeZoneWritable NOTIFY clockChanged)
    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY clockChanged)
    Q_PROPERTY(bool ntpPresent READ ntpPresent NOTIFY clockChanged)
    Q_PROPERTY(bool ntpEnabled READ ntpEnabled NOTIFY clockChanged)
    Q_PROPERTY(bool ntpRunning READ ntpRunning NOTIFY clockChanged)
    Q_PROPERTY(bool ntpWritable READ ntpWritable NOTIFY clockChanged)
    Q_PROPERTY(QString ntpStatus READ ntpStatus NOTIFY clockChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

public:
    explicit ClockController(QObject *parent = nullptr, QString systemRoot = {});

    // A configured NORTHSTAR_CLOCK_HELPER is authoritative, including when it
    // names nothing, so a test can say "no boundary here" as well as "this
    // one". Falling through to the real filesystem instead is what made an
    // earlier suite depend on what happened to be installed.
    static QString helperPath();

    // The synthetic region holding zones with no region of their own, such as
    // UTC and GMT.
    static QString otherRegion();

    QString timeZone() const;
    QString timeZoneStatus() const;
    bool timeZoneKnown() const;
    bool timeZoneWritable() const;
    QString region() const;
    bool ntpPresent() const;
    bool ntpEnabled() const;
    bool ntpRunning() const;
    bool ntpWritable() const;
    QString ntpStatus() const;
    QString status() const;
    bool statusIsError() const;

    // Regions are the first component of a zone name; zones are everything
    // beneath one, which can be two levels deep (America/Indiana/Knox).
    Q_INVOKABLE QStringList regions() const;
    Q_INVOKABLE QStringList zonesIn(const QString &region) const;

    // What a surface should offer: the zones in the region being browsed,
    // plus the zone actually in effect. Browsing is not the same act as
    // choosing, and a control that dropped the current zone while the user
    // looked at another region would report the timezone as unset when it is
    // nothing of the kind.
    Q_INVOKABLE QStringList selectableZones() const;
    Q_INVOKABLE bool isKnownZone(const QString &zone) const;

public slots:
    bool setTimeZone(const QString &zone);
    bool setNtpEnabled(bool enabled);
    bool synchroniseNow();
    void setRegion(const QString &region);
    void refresh();

signals:
    void clockChanged();
    void statusChanged();

private:
    struct HelperResult
    {
        bool started = false;
        int exitCode = -1;
        QString standardOutput;
    };

    HelperResult runHelper(const QStringList &arguments) const;
    QString zoneinfoPath() const;
    QString recordedZonePath() const;
    void readState();
    void announce(const QString &message, bool error);

    QString m_systemRoot;
    QString m_timeZone;
    QString m_timeZoneStatus;
    QString m_region;
    bool m_ntpPresent = false;
    bool m_ntpEnabled = false;
    bool m_ntpRunning = false;
    bool m_writable = false;
    QString m_ntpStatus;
    QString m_status;
    bool m_statusIsError = false;
};
