#include "applicationlauncher.h"
#include "desktoplayoutcontroller.h"
#include "desktopsettings.h"
#include "notificationcenter.h"
#include "pinnedapplicationmodel.h"
#include "quicksettingscontroller.h"
#include "sessioncontroller.h"
#include "settingscatalog.h"
#include "shellstate.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Proves the declaration in desktopsettings.cpp is wired to the controllers it
// claims, and that a capability the system does not provide is reported as
// unavailable instead of being offered as a control that does nothing.
class DesktopSettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void registersEverySection();
    void registersNoDeadControls();
    void togglesAppearanceThroughShellState();
    void togglesFilesGridViewThroughShellState();
    void reportsMissingSoundAsUnavailableWithTheMixerReason();
    void reportsMissingWirelessHardwareHonestly();
    void refusesSessionActionsWhenTheSessionIsNotSupervised();
    void resetsTheDesktopLayoutThroughItsController();
    void findsRepresentativeSettingsBySearch();

private:
    QString path(const QString &name) const;

    QTemporaryDir *m_directory = nullptr;
};

namespace {

// A system with no mixer, no wireless, no Bluetooth, and no brightness
// control: every probe fails to start.
QuickSettingsController::CommandProvider unequippedSystem()
{
    return [](const QString &, const QStringList &) {
        QuickSettingsCommandResult result;
        result.started = false;
        result.exitCode = -1;
        return result;
    };
}

} // namespace

void DesktopSettingsTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void DesktopSettingsTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString DesktopSettingsTest::path(const QString &name) const
{
    return m_directory->filePath(name);
}

// A fixture holding every controller the declaration routes to.
struct Desktop
{
    explicit Desktop(const QString &root)
        : shellState(nullptr, QDir(root).filePath(QStringLiteral("shell.ini")))
        , quickSettings(nullptr, QDir(root).filePath(QStringLiteral("quick.ini")),
                        unequippedSystem())
        , notifications(nullptr, 40, QDir(root).filePath(QStringLiteral("notifications.ini")))
        , desktopLayout(nullptr, QDir(root).filePath(QStringLiteral("layout.ini")))
        , launcher(nullptr, {}, {QDir(root).filePath(QStringLiteral("applications"))},
                   QDir(root).filePath(QStringLiteral("launch.log")),
                   {QDir(root).filePath(QStringLiteral("bundles"))},
                   QDir(root).filePath(QStringLiteral("associations.ini")))
        , pinned(nullptr, QDir(root).filePath(QStringLiteral("pinned.ini")))
        , session(QDir(root).filePath(QStringLiteral("absent-status")),
                  QDir(root).filePath(QStringLiteral("absent-control")),
                  0)
    {
        registerDesktopSettings(&catalog, &shellState, &quickSettings, &notifications,
                                &desktopLayout, &launcher, &pinned, &session);
    }

    SettingsCatalog catalog;
    ShellState shellState;
    QuickSettingsController quickSettings;
    NotificationCenter notifications;
    DesktopLayoutController desktopLayout;
    ApplicationLauncher launcher;
    PinnedApplicationModel pinned;
    SessionController session;
};

void DesktopSettingsTest::registersEverySection()
{
    Desktop desktop(m_directory->path());

    QStringList sectionIds;
    for (const QVariant &value : desktop.catalog.sections()) {
        sectionIds.append(value.toMap().value(QStringLiteral("id")).toString());
    }

    QCOMPARE(sectionIds, QStringList({QStringLiteral("appearance"), QStringLiteral("desktop"),
                                      QStringLiteral("sound"), QStringLiteral("network"),
                                      QStringLiteral("notifications"), QStringLiteral("session"),
                                      QStringLiteral("about")}));

    // Every section must actually hold something, or it is a dead tab.
    for (const QVariant &value : desktop.catalog.sections()) {
        const QVariantMap section = value.toMap();
        QVERIFY2(section.value(QStringLiteral("count")).toInt() > 0,
                 qPrintable(section.value(QStringLiteral("id")).toString()));
    }

    QCOMPARE(desktop.catalog.selectedSection(), QStringLiteral("appearance"));
}

void DesktopSettingsTest::registersNoDeadControls()
{
    Desktop desktop(m_directory->path());
    QVERIFY(desktop.catalog.entryCount() > 0);

    // Walk every declared entry through the catalog's own description so a
    // malformed declaration cannot slip in silently.
    for (const QVariant &value : desktop.catalog.sections()) {
        desktop.catalog.setSelectedSection(value.toMap().value(QStringLiteral("id")).toString());
        for (const QVariant &entryValue : desktop.catalog.entries()) {
            const QVariantMap entry = entryValue.toMap();
            const QString id = entry.value(QStringLiteral("id")).toString();
            QVERIFY2(!entry.value(QStringLiteral("title")).toString().isEmpty(), qPrintable(id));
            QVERIFY2(!entry.value(QStringLiteral("description")).toString().isEmpty(),
                     qPrintable(id));

            const QString kind = entry.value(QStringLiteral("kind")).toString();
            QVERIFY2(kind == SettingsCatalog::toggleKind() || kind == SettingsCatalog::sliderKind()
                         || kind == SettingsCatalog::actionKind()
                         || kind == SettingsCatalog::infoKind(),
                     qPrintable(id));

            // An unavailable control has to say why.
            if (!entry.value(QStringLiteral("available")).toBool()) {
                QVERIFY2(!entry.value(QStringLiteral("unavailableReason")).toString().isEmpty(),
                         qPrintable(id));
            }
        }
    }
}

void DesktopSettingsTest::togglesAppearanceThroughShellState()
{
    Desktop desktop(m_directory->path());
    const bool before = desktop.shellState.darkMode();

    QVERIFY(desktop.catalog.setValue(QStringLiteral("appearance.dark"), !before));
    QCOMPARE(desktop.shellState.darkMode(), !before);
    QCOMPARE(desktop.catalog.entryFor(QStringLiteral("appearance.dark"))
                 .value(QStringLiteral("value")).toBool(),
             !before);

    // A change made elsewhere is reflected without telling the catalog.
    desktop.shellState.setDarkMode(before);
    QCOMPARE(desktop.catalog.entryFor(QStringLiteral("appearance.dark"))
                 .value(QStringLiteral("value")).toBool(),
             before);
}

void DesktopSettingsTest::togglesFilesGridViewThroughShellState()
{
    Desktop desktop(m_directory->path());
    const bool before = desktop.shellState.filesGridView();

    QVERIFY(desktop.catalog.setValue(QStringLiteral("desktop.filesgrid"), !before));
    QCOMPARE(desktop.shellState.filesGridView(), !before);
}

void DesktopSettingsTest::reportsMissingSoundAsUnavailableWithTheMixerReason()
{
    Desktop desktop(m_directory->path());
    QVERIFY(!desktop.quickSettings.soundAvailable());

    const QVariantMap volume = desktop.catalog.entryFor(QStringLiteral("sound.volume"));
    QCOMPARE(volume.value(QStringLiteral("kind")).toString(), SettingsCatalog::sliderKind());
    QVERIFY(!volume.value(QStringLiteral("available")).toBool());
    QCOMPARE(volume.value(QStringLiteral("unavailableReason")).toString(),
             desktop.quickSettings.soundStatus());

    // The control refuses rather than pretending it applied a value.
    QVERIFY(!desktop.catalog.setValue(QStringLiteral("sound.volume"), 70));
    QVERIFY(desktop.catalog.statusIsError());
}

void DesktopSettingsTest::reportsMissingWirelessHardwareHonestly()
{
    Desktop desktop(m_directory->path());

    const QVariantMap wifi = desktop.catalog.entryFor(QStringLiteral("network.wifi"));
    QVERIFY(!wifi.value(QStringLiteral("available")).toBool());
    QVERIFY(!wifi.value(QStringLiteral("writable")).toBool());
    QCOMPARE(wifi.value(QStringLiteral("value")).toString(), desktop.quickSettings.wifiStatus());

    const QVariantMap bluetooth = desktop.catalog.entryFor(QStringLiteral("network.bluetooth"));
    QVERIFY(!bluetooth.value(QStringLiteral("available")).toBool());
    QCOMPARE(bluetooth.value(QStringLiteral("value")).toString(),
             desktop.quickSettings.bluetoothStatus());
}

void DesktopSettingsTest::refusesSessionActionsWhenTheSessionIsNotSupervised()
{
    Desktop desktop(m_directory->path());
    QVERIFY(!desktop.session.available());

    const QVariantMap restart = desktop.catalog.entryFor(QStringLiteral("session.restartshell"));
    QVERIFY(restart.value(QStringLiteral("destructive")).toBool());
    QVERIFY(!restart.value(QStringLiteral("available")).toBool());
    QVERIFY(!desktop.catalog.invoke(QStringLiteral("session.restartshell")));
    QVERIFY(desktop.catalog.statusIsError());
    QVERIFY(desktop.catalog.statusMessage().contains(QStringLiteral("not supervised")));

    const QVariantMap end = desktop.catalog.entryFor(QStringLiteral("session.end"));
    QVERIFY(end.value(QStringLiteral("destructive")).toBool());
    QVERIFY(!end.value(QStringLiteral("available")).toBool());
    QVERIFY(!desktop.catalog.invoke(QStringLiteral("session.end")));

    QCOMPARE(desktop.catalog.entryFor(QStringLiteral("session.state"))
                 .value(QStringLiteral("value")).toString(),
             QStringLiteral("Not supervised"));
}

void DesktopSettingsTest::resetsTheDesktopLayoutThroughItsController()
{
    Desktop desktop(m_directory->path());
    QVERIFY(desktop.desktopLayout.setPosition(QStringLiteral("/tmp/northstar-icon"), 120, 240));
    QVERIFY(!desktop.desktopLayout.positions().isEmpty());

    QVERIFY(desktop.catalog.invoke(QStringLiteral("desktop.resetlayout")));
    QVERIFY(desktop.desktopLayout.positions().isEmpty());
    QVERIFY(!desktop.catalog.statusIsError());
}

void DesktopSettingsTest::findsRepresentativeSettingsBySearch()
{
    Desktop desktop(m_directory->path());

    const auto firstResultId = [&desktop](const QString &query) {
        desktop.catalog.setQuery(query);
        const QVariantList results = desktop.catalog.entries();
        return results.isEmpty() ? QString()
                                 : results.first().toMap().value(QStringLiteral("id")).toString();
    };

    // Words a user would actually type, none of which is the setting's id.
    QCOMPARE(firstResultId(QStringLiteral("dark")), QStringLiteral("appearance.dark"));
    QCOMPARE(firstResultId(QStringLiteral("volume")), QStringLiteral("sound.volume"));
    QCOMPARE(firstResultId(QStringLiteral("quiet")), QStringLiteral("notifications.donotdisturb"));
    QCOMPARE(firstResultId(QStringLiteral("log out")), QStringLiteral("session.end"));
    QCOMPARE(firstResultId(QStringLiteral("wireless")), QStringLiteral("network.wifi"));

    // Searching reaches sections other than the selected one.
    QCOMPARE(desktop.catalog.selectedSection(), QStringLiteral("appearance"));
    desktop.catalog.setQuery(QStringLiteral("restart shell"));
    QVERIFY(desktop.catalog.resultCount() >= 1);
    QCOMPARE(desktop.catalog.entries().first().toMap()
                 .value(QStringLiteral("sectionLabel")).toString(),
             QStringLiteral("Session"));

    desktop.catalog.setQuery(QStringLiteral("nonexistent gibberish"));
    QCOMPARE(desktop.catalog.resultCount(), 0);
}

QTEST_MAIN(DesktopSettingsTest)
#include "test-desktopsettings.moc"
