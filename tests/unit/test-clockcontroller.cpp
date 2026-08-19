#include "clockcontroller.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// The controller reads a system root and drives a boundary, so both are
// pinned: a temporary tree stands in for the zoneinfo database, and
// NORTHSTAR_CLOCK_HELPER names a stub. Neither the machine's real timezone
// nor whether it happens to have the helper installed can affect a result.
class ClockControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listsRegionsFromTheDatabaseWithoutTheAlternateCopies();
    void listsZonesIncludingOnesNestedAFurtherLevel();
    void gathersRegionlessZonesUnderOneName();
    void refusesAZoneTheSystemDoesNotHave();
    void refusesAZoneNameThatEscapesTheDatabase();
    void reportsAnUnrecordedTimezoneRatherThanShowingNothing();
    void readsTheRecordedTimezone();
    void leavesEverythingReadOnlyWithoutTheBoundary();
    void writesTheTimezoneThroughTheBoundary();
    void reportsABoundaryThatRefusesTheChange();
    void togglesNetworkTimeThroughTheBoundary();
    void refusesAOneShotSyncWhileTheDaemonIsRunning();
    void keepsOfferingTheZoneInEffectWhileBrowsingAnotherRegion();
    void doesNotWaitForTheClockToBeSetFromTheNetwork();
    void reportsAOneShotCorrectionThatFailed();

private:
    QString root() const;
    void seedDatabase() const;
    void recordZone(const QString &zone) const;
    void installHelper(const QString &body) const;

    QTemporaryDir *m_directory = nullptr;
};

void ClockControllerTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
    // Absent by default. A case that wants a boundary installs one.
    qputenv("NORTHSTAR_CLOCK_HELPER", QByteArrayLiteral("/nonexistent/northstar-clock"));
    seedDatabase();
}

void ClockControllerTest::cleanup()
{
    qunsetenv("NORTHSTAR_CLOCK_HELPER");
    delete m_directory;
    m_directory = nullptr;
}

QString ClockControllerTest::root() const
{
    return m_directory->filePath(QStringLiteral("system"));
}

void ClockControllerTest::seedDatabase() const
{
    const QString database = QDir(root()).filePath(QStringLiteral("usr/share/zoneinfo"));
    const QStringList zones{QStringLiteral("America/Denver"),
                            QStringLiteral("America/New_York"),
                            QStringLiteral("America/Indiana/Knox"),
                            QStringLiteral("Europe/London"),
                            QStringLiteral("UTC"),
                            QStringLiteral("GMT"),
                            QStringLiteral("posix/America/Denver"),
                            QStringLiteral("right/America/Denver")};
    for (const QString &zone : zones) {
        const QString path = QDir(database).filePath(zone);
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("TZif");
        file.close();
    }

    for (const QString &index : {QStringLiteral("zone.tab"), QStringLiteral("tzdata.zi"),
                                 QStringLiteral("leapseconds")}) {
        QFile file(QDir(database).filePath(index));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# not a zone\n");
        file.close();
    }
}

void ClockControllerTest::recordZone(const QString &zone) const
{
    const QString path = QDir(root()).filePath(QStringLiteral("var/db/zoneinfo"));
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(zone.toUtf8());
    file.write("\n");
    file.close();
}

// The stub records what it was asked to do, so a case can assert the boundary
// received the action rather than only that the controller returned true.
void ClockControllerTest::installHelper(const QString &body) const
{
    const QString path = m_directory->filePath(QStringLiteral("northstar-clock"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\n");
    file.write(body.toUtf8());
    file.close();
    QVERIFY(QFile::setPermissions(path,
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    qputenv("NORTHSTAR_CLOCK_HELPER", path.toUtf8());
}

void ClockControllerTest::listsRegionsFromTheDatabaseWithoutTheAlternateCopies()
{
    ClockController clock(nullptr, root());

    // posix and right are complete copies of the tree. Offering them would
    // present every zone three times over.
    QCOMPARE(clock.regions(),
             QStringList({QStringLiteral("America"), QStringLiteral("Europe"),
                          ClockController::otherRegion()}));
}

void ClockControllerTest::listsZonesIncludingOnesNestedAFurtherLevel()
{
    ClockController clock(nullptr, root());

    QCOMPARE(clock.zonesIn(QStringLiteral("America")),
             QStringList({QStringLiteral("America/Denver"),
                          QStringLiteral("America/Indiana/Knox"),
                          QStringLiteral("America/New_York")}));
    QCOMPARE(clock.zonesIn(QStringLiteral("Europe")),
             QStringList({QStringLiteral("Europe/London")}));

    // The alternate copies are not reachable as regions either.
    QVERIFY(clock.zonesIn(QStringLiteral("posix")).isEmpty());
    QVERIFY(clock.zonesIn(QStringLiteral("right")).isEmpty());
}

void ClockControllerTest::gathersRegionlessZonesUnderOneName()
{
    ClockController clock(nullptr, root());

    // UTC and GMT are zones with no region. Index files sitting beside them
    // are not.
    const QStringList other = clock.zonesIn(ClockController::otherRegion());
    QVERIFY(other.contains(QStringLiteral("UTC")));
    QVERIFY(other.contains(QStringLiteral("GMT")));
    QVERIFY(!other.contains(QStringLiteral("zone.tab")));
    QVERIFY(!other.contains(QStringLiteral("tzdata.zi")));
    QVERIFY(!other.contains(QStringLiteral("leapseconds")));
}

void ClockControllerTest::refusesAZoneTheSystemDoesNotHave()
{
    installHelper(QStringLiteral("exit 0\n"));
    ClockController clock(nullptr, root());

    QVERIFY(!clock.setTimeZone(QStringLiteral("Mars/Olympus_Mons")));
    QVERIFY(clock.statusIsError());
    QVERIFY(clock.timeZone().isEmpty());
}

void ClockControllerTest::refusesAZoneNameThatEscapesTheDatabase()
{
    installHelper(QStringLiteral("exit 0\n"));
    ClockController clock(nullptr, root());

    // Resolved against the database rather than taken as a string, so a name
    // that climbs out of it is not a zone this system has.
    QVERIFY(!clock.isKnownZone(QStringLiteral("../../../etc/passwd")));
    QVERIFY(!clock.setTimeZone(QStringLiteral("../../../etc/passwd")));
    QVERIFY(clock.statusIsError());
}

void ClockControllerTest::reportsAnUnrecordedTimezoneRatherThanShowingNothing()
{
    // This is the state the development VM was actually in: a correct offset
    // from /etc/localtime with no record of which zone produced it.
    ClockController clock(nullptr, root());

    QVERIFY(!clock.timeZoneKnown());
    QVERIFY(!clock.timeZoneStatus().isEmpty());
    // A region is still chosen so the surface has zones to list.
    QVERIFY(!clock.region().isEmpty());
}

void ClockControllerTest::readsTheRecordedTimezone()
{
    recordZone(QStringLiteral("America/Denver"));
    ClockController clock(nullptr, root());

    QVERIFY(clock.timeZoneKnown());
    QCOMPARE(clock.timeZone(), QStringLiteral("America/Denver"));
    QVERIFY(clock.timeZoneStatus().isEmpty());
    // The region follows from the zone, so the surface opens on the right one.
    QCOMPARE(clock.region(), QStringLiteral("America"));
}

void ClockControllerTest::leavesEverythingReadOnlyWithoutTheBoundary()
{
    recordZone(QStringLiteral("Europe/London"));
    ClockController clock(nullptr, root());

    // Reading the clock needs no authority, so it is still reported.
    QCOMPARE(clock.timeZone(), QStringLiteral("Europe/London"));

    QVERIFY(!clock.timeZoneWritable());
    QVERIFY(!clock.ntpWritable());
    QVERIFY(!clock.ntpStatus().isEmpty());

    QVERIFY(!clock.setTimeZone(QStringLiteral("America/Denver")));
    QVERIFY(!clock.setNtpEnabled(true));
    QVERIFY(!clock.synchroniseNow());
    QCOMPARE(clock.timeZone(), QStringLiteral("Europe/London"));
}

void ClockControllerTest::writesTheTimezoneThroughTheBoundary()
{
    const QString recordPath = QDir(root()).filePath(QStringLiteral("var/db/zoneinfo"));
    QVERIFY(QDir().mkpath(QFileInfo(recordPath).absolutePath()));

    // Stands in for the real boundary: records the zone it was given, and
    // reports state the way the real one does.
    installHelper(QStringLiteral(
        "case \"$1\" in\n"
        "  timezone) printf '%s\\n' \"$2\" > '")
                  + recordPath
                  + QStringLiteral("'; exit 0 ;;\n"
                                   "  state)\n"
                                   "    printf 'ntp_present=yes\\n'\n"
                                   "    printf 'ntp_enabled=no\\n'\n"
                                   "    printf 'ntp_running=no\\n'\n"
                                   "    exit 0 ;;\n"
                                   "esac\n"
                                   "exit 64\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.timeZoneWritable());

    QSignalSpy changed(&clock, &ClockController::clockChanged);
    QVERIFY(clock.setTimeZone(QStringLiteral("America/Indiana/Knox")));
    QVERIFY(changed.count() >= 1);
    QCOMPARE(clock.timeZone(), QStringLiteral("America/Indiana/Knox"));
    QVERIFY(!clock.statusIsError());
}

void ClockControllerTest::reportsABoundaryThatRefusesTheChange()
{
    installHelper(QStringLiteral("case \"$1\" in\n"
                                 "  state) printf 'ntp_present=yes\\n'; exit 0 ;;\n"
                                 "esac\n"
                                 "exit 70\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.timeZoneWritable());

    // The zone is real and the boundary is installed; the boundary itself
    // refuses. That has to surface as a failure, not a silent success.
    QVERIFY(!clock.setTimeZone(QStringLiteral("Europe/London")));
    QVERIFY(clock.statusIsError());
    QVERIFY(!clock.timeZoneKnown());
}

void ClockControllerTest::togglesNetworkTimeThroughTheBoundary()
{
    const QString statePath = m_directory->filePath(QStringLiteral("ntp-state"));
    QFile initial(statePath);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write("no\n");
    initial.close();

    installHelper(QStringLiteral(
        "case \"$1\" in\n"
        "  ntp) printf '%s\\n' \"$2\" > '")
                  + statePath
                  + QStringLiteral("'; exit 0 ;;\n"
                                   "  state)\n"
                                   "    printf 'ntp_present=yes\\n'\n"
                                   "    if [ \"$(cat '")
                  + statePath
                  + QStringLiteral("')\" = on ]; then\n"
                                   "      printf 'ntp_enabled=yes\\n'\n"
                                   "      printf 'ntp_running=yes\\n'\n"
                                   "    else\n"
                                   "      printf 'ntp_enabled=no\\n'\n"
                                   "      printf 'ntp_running=no\\n'\n"
                                   "    fi\n"
                                   "    exit 0 ;;\n"
                                   "esac\n"
                                   "exit 64\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.ntpWritable());
    QVERIFY(!clock.ntpEnabled());

    QVERIFY(clock.setNtpEnabled(true));
    QVERIFY(clock.ntpEnabled());
    QVERIFY(clock.ntpRunning());
    QCOMPARE(clock.ntpStatus(), QStringLiteral("Running"));

    QVERIFY(clock.setNtpEnabled(false));
    QVERIFY(!clock.ntpEnabled());
    QCOMPARE(clock.ntpStatus(), QStringLiteral("Off"));
}

void ClockControllerTest::refusesAOneShotSyncWhileTheDaemonIsRunning()
{
    installHelper(QStringLiteral("case \"$1\" in\n"
                                 "  state)\n"
                                 "    printf 'ntp_present=yes\\n'\n"
                                 "    printf 'ntp_enabled=yes\\n'\n"
                                 "    printf 'ntp_running=yes\\n'\n"
                                 "    exit 0 ;;\n"
                                 "esac\n"
                                 "exit 0\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.ntpRunning());

    // A one-shot step cannot bind the port the daemon holds, and there is
    // nothing for it to do anyway.
    QVERIFY(!clock.synchroniseNow());
    QVERIFY(clock.statusIsError());
}

void ClockControllerTest::keepsOfferingTheZoneInEffectWhileBrowsingAnotherRegion()
{
    recordZone(QStringLiteral("America/Denver"));
    ClockController clock(nullptr, root());
    QCOMPARE(clock.timeZone(), QStringLiteral("America/Denver"));
    QCOMPARE(clock.region(), QStringLiteral("America"));

    // Browsing is not choosing. Looking at another region must not make the
    // surface report that no timezone is set, which is what happens when the
    // zone in effect is missing from the list the control offers.
    clock.setRegion(QStringLiteral("Europe"));

    const QStringList offered = clock.selectableZones();
    QVERIFY(offered.contains(QStringLiteral("Europe/London")));
    QVERIFY2(offered.contains(QStringLiteral("America/Denver")),
             "the zone in effect disappeared while another region was browsed");
    QCOMPARE(clock.timeZone(), QStringLiteral("America/Denver"));

    // Back in its own region it is offered exactly once.
    clock.setRegion(QStringLiteral("America"));
    QCOMPARE(clock.selectableZones().count(QStringLiteral("America/Denver")), 1);
}

void ClockControllerTest::doesNotWaitForTheClockToBeSetFromTheNetwork()
{
    // A time server is across a network, so the step takes seconds. This
    // stub stands in for that. If the controller waits for it, every shell
    // surface is frozen for the duration, which is what the walkthrough hit.
    installHelper(QStringLiteral("case \"$1\" in\n"
                                 "  state)\n"
                                 "    printf 'ntp_present=yes\\n'\n"
                                 "    printf 'ntp_enabled=no\\n'\n"
                                 "    printf 'ntp_running=no\\n'\n"
                                 "    exit 0 ;;\n"
                                 "  sync) sleep 2; exit 0 ;;\n"
                                 "esac\n"
                                 "exit 64\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.ntpWritable());
    QVERIFY(!clock.synchronising());

    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY(clock.synchroniseNow());
    const qint64 returnedAfter = elapsed.elapsed();

    // The call has to come back promptly, having only started the work.
    QVERIFY2(returnedAfter < 1000,
             qPrintable(QStringLiteral("synchroniseNow blocked for %1 ms").arg(returnedAfter)));
    QVERIFY(clock.synchronising());

    // A second request while one is in flight is refused rather than queued.
    QVERIFY(!clock.synchroniseNow());

    QSignalSpy changed(&clock, &ClockController::clockChanged);
    QVERIFY(QTest::qWaitFor([&clock]() { return !clock.synchronising(); }, 15000));
    QVERIFY(changed.count() >= 1);
    QVERIFY(!clock.statusIsError());
}

void ClockControllerTest::reportsAOneShotCorrectionThatFailed()
{
    // A system that cannot reach its time servers must be told so, not left
    // believing the clock was set.
    installHelper(QStringLiteral("case \"$1\" in\n"
                                 "  state)\n"
                                 "    printf 'ntp_present=yes\\n'\n"
                                 "    printf 'ntp_enabled=no\\n'\n"
                                 "    printf 'ntp_running=no\\n'\n"
                                 "    exit 0 ;;\n"
                                 "  sync) exit 1 ;;\n"
                                 "esac\n"
                                 "exit 64\n"));

    ClockController clock(nullptr, root());
    QVERIFY(clock.synchroniseNow());
    QVERIFY(QTest::qWaitFor([&clock]() { return !clock.synchronising(); }, 15000));
    QVERIFY(clock.statusIsError());
    QVERIFY(clock.status().contains(QStringLiteral("could not be set")));
}

QTEST_MAIN(ClockControllerTest)
#include "test-clockcontroller.moc"
