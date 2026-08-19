#include "clockcontroller.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr int HelperTimeoutMilliseconds = 15000;

const QLatin1String OtherRegionName("Other");

// Directories in the zoneinfo database that are not regions. posix and right
// are alternate copies of the whole tree, so including them would offer every
// zone three times over.
bool isRegionDirectory(const QString &name)
{
    return name != QLatin1String("posix") && name != QLatin1String("right");
}

// Files in the database that are indexes or metadata rather than zones.
bool isZoneFile(const QString &name)
{
    if (name.endsWith(QLatin1String(".tab")) || name.endsWith(QLatin1String(".zi"))
        || name.endsWith(QLatin1String(".list"))) {
        return false;
    }
    return name != QLatin1String("leapseconds") && name != QLatin1String("Factory")
        && name != QLatin1String("localtime") && name != QLatin1String("posixrules");
}

QString valueFor(const QStringList &lines, const QString &key)
{
    const QString prefix = key + QLatin1Char('=');
    for (const QString &line : lines) {
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size()).trimmed();
        }
    }
    return QString();
}

} // namespace

ClockController::ClockController(QObject *parent, QString systemRoot)
    : QObject(parent)
    , m_systemRoot(systemRoot.trimmed().isEmpty()
            ? QStringLiteral("/")
            : QDir::cleanPath(QDir::fromNativeSeparators(systemRoot)))
{
    readState();
}

QString ClockController::helperPath()
{
    const QString configuredPath = qEnvironmentVariable("NORTHSTAR_CLOCK_HELPER");
    if (!configuredPath.isEmpty()) {
        return QFileInfo(configuredPath).isExecutable() ? configuredPath : QString();
    }
    const QString userPath = QDir::home().filePath(QStringLiteral(".local/bin/northstar-clock"));
    if (QFileInfo(userPath).isExecutable()) {
        return userPath;
    }
    return QStandardPaths::findExecutable(QStringLiteral("northstar-clock"));
}

QString ClockController::otherRegion()
{
    return OtherRegionName;
}

QString ClockController::zoneinfoPath() const
{
    return QDir(m_systemRoot).filePath(QStringLiteral("usr/share/zoneinfo"));
}

QString ClockController::recordedZonePath() const
{
    return QDir(m_systemRoot).filePath(QStringLiteral("var/db/zoneinfo"));
}

QString ClockController::timeZone() const
{
    return m_timeZone;
}

QString ClockController::timeZoneStatus() const
{
    return m_timeZoneStatus;
}

bool ClockController::timeZoneKnown() const
{
    return !m_timeZone.isEmpty();
}

bool ClockController::timeZoneWritable() const
{
    return m_writable;
}

QString ClockController::region() const
{
    return m_region;
}

bool ClockController::ntpPresent() const
{
    return m_ntpPresent;
}

bool ClockController::ntpEnabled() const
{
    return m_ntpEnabled;
}

bool ClockController::ntpRunning() const
{
    return m_ntpRunning;
}

bool ClockController::ntpWritable() const
{
    return m_writable && m_ntpPresent;
}

QString ClockController::ntpStatus() const
{
    return m_ntpStatus;
}

QString ClockController::status() const
{
    return m_status;
}

bool ClockController::statusIsError() const
{
    return m_statusIsError;
}

QStringList ClockController::regions() const
{
    QStringList found;
    const QDir database(zoneinfoPath());
    if (!database.exists()) {
        return found;
    }

    bool sawBareZone = false;
    const QFileInfoList entries =
        database.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            if (isRegionDirectory(entry.fileName())) {
                found.append(entry.fileName());
            }
        } else if (isZoneFile(entry.fileName())) {
            sawBareZone = true;
        }
    }

    // Zones such as UTC sit at the top of the database with no region of
    // their own. They are still zones, so they get a name to live under
    // rather than being dropped from the surface.
    if (sawBareZone) {
        found.append(OtherRegionName);
    }
    return found;
}

QStringList ClockController::zonesIn(const QString &region) const
{
    QStringList found;
    const QString trimmed = region.trimmed();
    if (trimmed.isEmpty()) {
        return found;
    }

    const QDir database(zoneinfoPath());
    if (!database.exists()) {
        return found;
    }

    if (trimmed == OtherRegionName) {
        const QFileInfoList entries =
            database.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (isZoneFile(entry.fileName())) {
                found.append(entry.fileName());
            }
        }
        return found;
    }

    if (!isRegionDirectory(trimmed)) {
        return found;
    }

    const QString regionPath = database.filePath(trimmed);
    if (!QFileInfo(regionPath).isDir()) {
        return found;
    }

    // A region can nest one level further, as America/Indiana/Knox does, so
    // this walks rather than lists.
    QDirIterator iterator(regionPath, QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    const QDir regionDirectory(regionPath);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString relative = regionDirectory.relativeFilePath(path);
        if (isZoneFile(QFileInfo(path).fileName())) {
            found.append(trimmed + QLatin1Char('/') + relative);
        }
    }
    found.sort();
    return found;
}

QStringList ClockController::selectableZones() const
{
    QStringList zones = zonesIn(m_region);
    if (!m_timeZone.isEmpty() && !zones.contains(m_timeZone)) {
        zones.append(m_timeZone);
        zones.sort();
    }
    return zones;
}

bool ClockController::isKnownZone(const QString &zone) const
{
    const QString trimmed = zone.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    // Resolve against the database rather than trusting the string: a name
    // that escapes the database is not a zone this system has.
    const QDir database(zoneinfoPath());
    const QString candidate = QDir::cleanPath(database.filePath(trimmed));
    const QString root = QDir::cleanPath(database.absolutePath());
    if (!candidate.startsWith(root + QLatin1Char('/'))) {
        return false;
    }
    return QFileInfo(candidate).isFile();
}

void ClockController::setRegion(const QString &region)
{
    const QString trimmed = region.trimmed();
    if (m_region == trimmed) {
        return;
    }
    m_region = trimmed;
    emit clockChanged();
}

bool ClockController::setTimeZone(const QString &zone)
{
    const QString trimmed = zone.trimmed();
    if (!m_writable) {
        announce(QStringLiteral("The clock boundary is not installed, so the timezone "
                                "cannot be changed here."),
                 true);
        return false;
    }
    if (!isKnownZone(trimmed)) {
        announce(QStringLiteral("%1 is not a timezone this system has.")
                     .arg(trimmed.isEmpty() ? QStringLiteral("That name") : trimmed),
                 true);
        return false;
    }

    const HelperResult result = runHelper({QStringLiteral("timezone"), trimmed});
    if (!result.started || result.exitCode != 0) {
        announce(QStringLiteral("The timezone could not be changed to %1.").arg(trimmed), true);
        return false;
    }

    readState();
    announce(QStringLiteral("Timezone set to %1.").arg(trimmed), false);
    emit clockChanged();
    return true;
}

bool ClockController::setNtpEnabled(bool enabled)
{
    if (!ntpWritable()) {
        announce(m_ntpStatus.isEmpty()
                     ? QStringLiteral("Network time cannot be changed on this system.")
                     : m_ntpStatus,
                 true);
        return false;
    }

    const HelperResult result = runHelper(
        {QStringLiteral("ntp"), enabled ? QStringLiteral("on") : QStringLiteral("off")});
    if (!result.started || result.exitCode != 0) {
        announce(enabled ? QStringLiteral("Network time could not be started.")
                         : QStringLiteral("Network time could not be stopped."),
                 true);
        return false;
    }

    readState();
    announce(enabled ? QStringLiteral("Network time is on.")
                     : QStringLiteral("Network time is off."),
             false);
    emit clockChanged();
    return true;
}

bool ClockController::synchroniseNow()
{
    if (!ntpWritable()) {
        announce(m_ntpStatus.isEmpty()
                     ? QStringLiteral("The clock cannot be set from the network on this system.")
                     : m_ntpStatus,
                 true);
        return false;
    }
    if (m_ntpRunning) {
        // Asking for a one-shot step while the daemon is already keeping the
        // clock set is a request with nothing to do.
        announce(QStringLiteral("Network time is already running and keeping the clock set."),
                 true);
        return false;
    }

    const HelperResult result = runHelper({QStringLiteral("sync")});
    if (!result.started || result.exitCode != 0) {
        announce(QStringLiteral("The clock could not be set from the network. "
                                "Check that this system can reach its time servers."),
                 true);
        return false;
    }

    readState();
    announce(QStringLiteral("Clock set from the network."), false);
    emit clockChanged();
    return true;
}

void ClockController::refresh()
{
    readState();
    emit clockChanged();
}

ClockController::HelperResult ClockController::runHelper(const QStringList &arguments) const
{
    HelperResult result;
    const QString helper = helperPath();
    if (helper.isEmpty()) {
        return result;
    }

    QProcess process;
    process.setProgram(helper);
    process.setArguments(arguments);
    process.start(QIODevice::ReadOnly);

    result.started = process.waitForStarted(HelperTimeoutMilliseconds);
    if (!result.started) {
        return result;
    }
    if (!process.waitForFinished(HelperTimeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(HelperTimeoutMilliseconds);
        result.started = false;
        return result;
    }
    result.exitCode = process.exitCode();
    result.standardOutput = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    return result;
}

void ClockController::readState()
{
    m_writable = !helperPath().isEmpty();
    m_ntpPresent = false;
    m_ntpEnabled = false;
    m_ntpRunning = false;

    // The recorded name is read directly rather than through the boundary, so
    // the timezone is still reportable on a system where no boundary is
    // installed at all.
    m_timeZone.clear();
    QFile recorded(recordedZonePath());
    if (recorded.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString name = QString::fromLocal8Bit(recorded.readLine()).trimmed();
        if (isKnownZone(name)) {
            m_timeZone = name;
        }
        recorded.close();
    }

    if (m_timeZone.isEmpty()) {
        // The system still has an offset, from /etc/localtime; what it does
        // not have is a record of which zone produced it. Saying so is more
        // use than showing a blank field, and setting a zone fixes it.
        m_timeZoneStatus =
            QStringLiteral("This system has not recorded which timezone it is set to. "
                           "Choosing one below will record it.");
    } else {
        m_timeZoneStatus.clear();
        if (m_region.isEmpty()) {
            const int separator = m_timeZone.indexOf(QLatin1Char('/'));
            m_region = separator > 0 ? m_timeZone.left(separator) : OtherRegionName;
        }
    }

    // The surface always needs a region to list zones from, including on a
    // system that has not recorded its own timezone. Without this the zone
    // control would have nothing to offer and would be refused registration.
    if (m_region.isEmpty()) {
        m_region = regions().value(0);
    }

    if (!m_writable) {
        m_ntpStatus = QStringLiteral("The clock boundary is not installed on this system");
        return;
    }

    const HelperResult state = runHelper({QStringLiteral("state")});
    if (!state.started || state.exitCode != 0) {
        m_ntpStatus = QStringLiteral("The clock boundary did not report its state");
        return;
    }

    const QStringList lines = state.standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    m_ntpPresent = valueFor(lines, QStringLiteral("ntp_present")) == QLatin1String("yes");
    m_ntpEnabled = valueFor(lines, QStringLiteral("ntp_enabled")) == QLatin1String("yes");
    m_ntpRunning = valueFor(lines, QStringLiteral("ntp_running")) == QLatin1String("yes");

    // The boundary reads the recorded name too, and on a real system it is
    // reading the same file. Prefer it when the direct read found nothing,
    // which is what happens when the shell and the clock disagree about the
    // root they are looking at.
    if (m_timeZone.isEmpty()) {
        const QString reported = valueFor(lines, QStringLiteral("timezone"));
        if (isKnownZone(reported)) {
            m_timeZone = reported;
            m_timeZoneStatus.clear();
        }
    }

    if (!m_ntpPresent) {
        m_ntpStatus = QStringLiteral("No network time daemon is installed");
    } else if (m_ntpRunning) {
        m_ntpStatus = QStringLiteral("Running");
    } else if (m_ntpEnabled) {
        m_ntpStatus = QStringLiteral("Enabled but not running");
    } else {
        m_ntpStatus = QStringLiteral("Off");
    }
}

void ClockController::announce(const QString &message, bool error)
{
    if (m_status == message && m_statusIsError == error) {
        return;
    }
    m_status = message;
    m_statusIsError = error;
    emit statusChanged();
}
