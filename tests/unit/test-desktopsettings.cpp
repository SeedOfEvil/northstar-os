#include "applicationlauncher.h"
#include "desktoplayoutcontroller.h"
#include "desktopsettings.h"
#include "notificationcenter.h"
#include "pinnedapplicationmodel.h"
#include "quicksettingscontroller.h"
#include "sessioncontroller.h"
#include "settingscatalog.h"
#include "shellstate.h"
#include "wallpapercontroller.h"

#include <QDir>
#include <QFile>
#include <QImage>
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
    void offersRadioTogglesWhereControlIsInstalled();
    void declaresTheDesktopBackgroundAgainstTheWallpaperController();
    void offersNoFitUntilAPictureIsChosen();
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
    // Registration consults whether the radio boundary is installed, so pin it
    // to something absent. Without this the declared entries, and therefore
    // this suite, depend on whether the machine running it happens to have the
    // helper installed.
    qputenv("NORTHSTAR_RADIO_HELPER", QByteArrayLiteral("/nonexistent/northstar-radio"));
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void DesktopSettingsTest::cleanup()
{
    qunsetenv("NORTHSTAR_RADIO_HELPER");
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
        , wallpaper(nullptr, QDir(root).filePath(QStringLiteral("wallpaper.ini")))
    {
        registerDesktopSettings(&catalog, &shellState, &quickSettings, &notifications,
                                &desktopLayout, &launcher, &pinned, &session, &wallpaper);
    }

    SettingsCatalog catalog;
    ShellState shellState;
    QuickSettingsController quickSettings;
    NotificationCenter notifications;
    DesktopLayoutController desktopLayout;
    ApplicationLauncher launcher;
    PinnedApplicationModel pinned;
    SessionController session;
    WallpaperController wallpaper;
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
                         || kind == SettingsCatalog::choiceKind()
                         || kind == SettingsCatalog::pathKind()
                         || kind == SettingsCatalog::actionKind()
                         || kind == SettingsCatalog::infoKind(),
                     qPrintable(id));

            // A choice is dead in a way the kind check cannot see: it can be
            // well formed and still offer nothing, or offer a value its own
            // reader never returns.
            if (kind == SettingsCatalog::choiceKind()) {
                const QVariantList options = entry.value(QStringLiteral("options")).toList();
                QVERIFY2(!options.isEmpty(), qPrintable(id));
                QStringList values;
                for (const QVariant &option : options) {
                    const QVariantMap map = option.toMap();
                    QVERIFY2(!map.value(QStringLiteral("value")).toString().isEmpty(),
                             qPrintable(id));
                    QVERIFY2(!map.value(QStringLiteral("label")).toString().isEmpty(),
                             qPrintable(id));
                    values.append(map.value(QStringLiteral("value")).toString());
                }
                // Whatever it currently reads has to be one of the things it
                // offers, or the control opens showing nothing selected.
                QVERIFY2(values.contains(entry.value(QStringLiteral("value")).toString()),
                         qPrintable(id));
            }

            // A path entry has to say what it shows when nothing is chosen,
            // otherwise the control renders as an empty gap.
            if (kind == SettingsCatalog::pathKind()) {
                QVERIFY2(!entry.value(QStringLiteral("emptyLabel")).toString().isEmpty(),
                         qPrintable(id));
            }

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

void DesktopSettingsTest::declaresTheDesktopBackgroundAgainstTheWallpaperController()
{
    Desktop desktop(m_directory->path());

    const QVariantMap picture = desktop.catalog.entryFor(QStringLiteral("appearance.wallpaper"));
    QCOMPARE(picture.value(QStringLiteral("kind")).toString(), SettingsCatalog::pathKind());
    QVERIFY(picture.value(QStringLiteral("writable")).toBool());
    QVERIFY(picture.value(QStringLiteral("available")).toBool());
    QVERIFY(picture.value(QStringLiteral("value")).toString().isEmpty());
    QVERIFY(!picture.value(QStringLiteral("emptyLabel")).toString().isEmpty());

    // Writing through the catalog has to reach the controller that owns the
    // behaviour, and a refusal has to carry that controller's own reason.
    const QString absent = m_directory->filePath(QStringLiteral("nowhere.png"));
    QVERIFY(!desktop.catalog.setValue(QStringLiteral("appearance.wallpaper"), absent));
    QVERIFY(desktop.catalog.statusIsError());
    QCOMPARE(desktop.catalog.statusMessage(), desktop.wallpaper.status());

    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::green);
    const QString accepted = m_directory->filePath(QStringLiteral("chosen.png"));
    QVERIFY(image.save(accepted, "PNG"));

    QVERIFY(desktop.catalog.setValue(QStringLiteral("appearance.wallpaper"), accepted));
    QVERIFY(desktop.wallpaper.hasImage());
    QCOMPARE(desktop.catalog.entryFor(QStringLiteral("appearance.wallpaper"))
                 .value(QStringLiteral("value"))
                 .toString(),
             desktop.wallpaper.imagePath());
}

void DesktopSettingsTest::offersNoFitUntilAPictureIsChosen()
{
    Desktop desktop(m_directory->path());

    // Fit is declared as a choice, but it has nothing to act on while the
    // desktop is still on the built-in background.
    const QVariantMap fit = desktop.catalog.entryFor(QStringLiteral("appearance.wallpaperfit"));
    QCOMPARE(fit.value(QStringLiteral("kind")).toString(), SettingsCatalog::choiceKind());
    QVERIFY(!fit.value(QStringLiteral("available")).toBool());
    QVERIFY(!fit.value(QStringLiteral("unavailableReason")).toString().isEmpty());

    // Every fit the controller accepts is offered, and no others.
    QStringList offered;
    for (const QVariant &option : fit.value(QStringLiteral("options")).toList()) {
        offered.append(option.toMap().value(QStringLiteral("value")).toString());
    }
    QCOMPARE(offered, WallpaperController::fitModes());

    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString picture = m_directory->filePath(QStringLiteral("backdrop.png"));
    QVERIFY(image.save(picture, "PNG"));
    QVERIFY(desktop.wallpaper.setImagePath(picture));

    QVERIFY(desktop.catalog.entryFor(QStringLiteral("appearance.wallpaperfit"))
                .value(QStringLiteral("available"))
                .toBool());
    QVERIFY(desktop.catalog.setValue(QStringLiteral("appearance.wallpaperfit"),
                                     QStringLiteral("tile")));
    QCOMPARE(desktop.wallpaper.fitMode(), QStringLiteral("tile"));
}

void DesktopSettingsTest::offersRadioTogglesWhereControlIsInstalled()
{
    // The other half of the declaration: with the boundary installed the
    // radios are declared as controls rather than readings, while staying
    // unavailable because this fixture has no hardware.
    const QString helperPath = m_directory->filePath(QStringLiteral("northstar-radio"));
    QFile stub(helperPath);
    QVERIFY(stub.open(QIODevice::WriteOnly));
    stub.write("#!/bin/sh\nexit 0\n");
    stub.close();
    QVERIFY(QFile::setPermissions(helperPath,
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    qputenv("NORTHSTAR_RADIO_HELPER", helperPath.toUtf8());

    Desktop desktop(m_directory->path());

    const QVariantMap wifi = desktop.catalog.entryFor(QStringLiteral("network.wifi"));
    QCOMPARE(wifi.value(QStringLiteral("kind")).toString(), SettingsCatalog::toggleKind());
    QVERIFY(wifi.value(QStringLiteral("writable")).toBool());

    // Declared as a control, but still honest about the missing radio.
    QVERIFY(!wifi.value(QStringLiteral("available")).toBool());
    QCOMPARE(wifi.value(QStringLiteral("unavailableReason")).toString(),
             desktop.quickSettings.wifiStatus());

    const QVariantMap bluetooth = desktop.catalog.entryFor(QStringLiteral("network.bluetooth"));
    QCOMPARE(bluetooth.value(QStringLiteral("kind")).toString(), SettingsCatalog::toggleKind());
    QVERIFY(!bluetooth.value(QStringLiteral("available")).toBool());
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
