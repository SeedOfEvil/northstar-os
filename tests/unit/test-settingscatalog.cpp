#include "settingscatalog.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class SettingsCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void registersSectionsAndEntries();
    void rejectsEntriesThatCannotBackTheirKind();
    void listsOnlyTheSelectedSectionWhenNotSearching();
    void searchesAcrossEverySection();
    void requiresEveryTokenToMatch();
    void ranksTitleMatchesAboveDescriptionMatches();
    void ranksAvailableControlsAboveUnavailableOnes();
    void findsSettingsByKeywordAndSectionName();
    void writesTogglesThroughTheBackingController();
    void clampsSliderValuesToTheDeclaredRange();
    void refusesToWriteAnUnavailableControlAndStatesWhy();
    void refusesToWriteReadOnlyEntries();
    void refusesChoicesWithNothingToChooseFrom();
    void writesOnlyValuesAChoiceOffers();
    void writesPathsAndReportsTheControllersReason();
    void performsActions();
    void reportsAFailedAction();
    void reportsCurrentValuesOnEveryRead();
};

namespace {

// A minimal stand-in for the shell controllers the catalog routes to.
struct FakeDesktop
{
    bool darkMode = true;
    int volume = 40;
    bool soundAvailable = true;
    int resetCount = 0;
    bool resetSucceeds = true;
};

SettingsCatalog::Entry toggleEntry(const QString &id, const QString &section,
                                   const QString &title, bool *backing)
{
    SettingsCatalog::Entry entry;
    entry.id = id;
    entry.section = section;
    entry.title = title;
    entry.kind = SettingsCatalog::toggleKind();
    entry.read = [backing]() { return QVariant(*backing); };
    entry.write = [backing](const QVariant &value) {
        *backing = value.toBool();
        return true;
    };
    return entry;
}

SettingsCatalog::Entry infoEntry(const QString &id, const QString &section, const QString &title,
                                 const QString &value)
{
    SettingsCatalog::Entry entry;
    entry.id = id;
    entry.section = section;
    entry.title = title;
    entry.kind = SettingsCatalog::infoKind();
    entry.read = [value]() { return QVariant(value); };
    return entry;
}

} // namespace

void SettingsCatalogTest::registersSectionsAndEntries()
{
    SettingsCatalog catalog;
    bool dark = true;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    catalog.registerSection(QStringLiteral("session"), QStringLiteral("Session"));

    // The first registered section becomes the default selection.
    QCOMPARE(catalog.selectedSection(), QStringLiteral("appearance"));
    QCOMPARE(catalog.sections().size(), 2);

    QVERIFY(catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                              QStringLiteral("appearance"),
                                              QStringLiteral("Dark appearance"), &dark)));
    QCOMPARE(catalog.entryCount(), 1);
    QCOMPARE(catalog.sections().first().toMap().value(QStringLiteral("count")).toInt(), 1);

    // Duplicate ids and unknown sections are refused.
    QVERIFY(!catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                               QStringLiteral("appearance"),
                                               QStringLiteral("Dark appearance"), &dark)));
    QVERIFY(!catalog.registerEntry(toggleEntry(QStringLiteral("nowhere.toggle"),
                                               QStringLiteral("nowhere"),
                                               QStringLiteral("Nowhere"), &dark)));
    QCOMPARE(catalog.entryCount(), 1);
}

void SettingsCatalogTest::rejectsEntriesThatCannotBackTheirKind()
{
    SettingsCatalog catalog;
    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));

    // A toggle with no writer would render a control that does nothing.
    SettingsCatalog::Entry deadToggle;
    deadToggle.id = QStringLiteral("appearance.dead");
    deadToggle.section = QStringLiteral("appearance");
    deadToggle.title = QStringLiteral("Dead toggle");
    deadToggle.kind = SettingsCatalog::toggleKind();
    deadToggle.read = []() { return QVariant(true); };
    QVERIFY(!catalog.registerEntry(deadToggle));

    SettingsCatalog::Entry deadAction;
    deadAction.id = QStringLiteral("appearance.deadaction");
    deadAction.section = QStringLiteral("appearance");
    deadAction.title = QStringLiteral("Dead action");
    deadAction.kind = SettingsCatalog::actionKind();
    QVERIFY(!catalog.registerEntry(deadAction));

    SettingsCatalog::Entry unknownKind;
    unknownKind.id = QStringLiteral("appearance.unknown");
    unknownKind.section = QStringLiteral("appearance");
    unknownKind.title = QStringLiteral("Unknown");
    unknownKind.kind = QStringLiteral("carousel");
    unknownKind.read = []() { return QVariant(1); };
    QVERIFY(!catalog.registerEntry(unknownKind));

    // An inverted slider range is a declaration error, not a rendering problem.
    int level = 5;
    SettingsCatalog::Entry badRange;
    badRange.id = QStringLiteral("appearance.range");
    badRange.section = QStringLiteral("appearance");
    badRange.title = QStringLiteral("Range");
    badRange.kind = SettingsCatalog::sliderKind();
    badRange.minimum = 100;
    badRange.maximum = 0;
    badRange.read = [&level]() { return QVariant(level); };
    badRange.write = [&level](const QVariant &value) { level = value.toInt(); return true; };
    QVERIFY(!catalog.registerEntry(badRange));

    QCOMPARE(catalog.entryCount(), 0);
}

void SettingsCatalogTest::listsOnlyTheSelectedSectionWhenNotSearching()
{
    SettingsCatalog catalog;
    bool dark = true;
    bool grid = true;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    catalog.registerSection(QStringLiteral("desktop"), QStringLiteral("Desktop"));
    catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                      QStringLiteral("appearance"),
                                      QStringLiteral("Dark appearance"), &dark));
    catalog.registerEntry(toggleEntry(QStringLiteral("desktop.grid"), QStringLiteral("desktop"),
                                      QStringLiteral("Files grid view"), &grid));

    QVERIFY(!catalog.searching());
    QCOMPARE(catalog.resultCount(), 1);
    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("appearance.dark"));

    catalog.setSelectedSection(QStringLiteral("desktop"));
    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("desktop.grid"));

    // An unknown section is ignored rather than emptying the surface.
    catalog.setSelectedSection(QStringLiteral("nowhere"));
    QCOMPARE(catalog.selectedSection(), QStringLiteral("desktop"));
}

void SettingsCatalogTest::searchesAcrossEverySection()
{
    SettingsCatalog catalog;
    bool dark = true;
    bool grid = true;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    catalog.registerSection(QStringLiteral("desktop"), QStringLiteral("Desktop"));
    catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                      QStringLiteral("appearance"),
                                      QStringLiteral("Dark appearance"), &dark));
    catalog.registerEntry(toggleEntry(QStringLiteral("desktop.grid"), QStringLiteral("desktop"),
                                      QStringLiteral("Files grid view"), &grid));

    QCOMPARE(catalog.selectedSection(), QStringLiteral("appearance"));
    catalog.setQuery(QStringLiteral("grid"));
    QVERIFY(catalog.searching());

    // The match is found even though it lives outside the selected section.
    QCOMPARE(catalog.resultCount(), 1);
    const QVariantMap result = catalog.entries().first().toMap();
    QCOMPARE(result.value(QStringLiteral("id")).toString(), QStringLiteral("desktop.grid"));
    QCOMPARE(result.value(QStringLiteral("sectionLabel")).toString(), QStringLiteral("Desktop"));

    catalog.clearQuery();
    QVERIFY(!catalog.searching());
    QCOMPARE(catalog.resultCount(), 1);
    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("appearance.dark"));
}

void SettingsCatalogTest::requiresEveryTokenToMatch()
{
    SettingsCatalog catalog;
    bool dark = true;
    bool grid = true;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                      QStringLiteral("appearance"),
                                      QStringLiteral("Dark appearance"), &dark));
    catalog.registerEntry(toggleEntry(QStringLiteral("appearance.grid"),
                                      QStringLiteral("appearance"),
                                      QStringLiteral("Files grid view"), &grid));

    catalog.setQuery(QStringLiteral("dark"));
    QCOMPARE(catalog.resultCount(), 1);

    // "dark grid" describes no single setting, so it must return nothing
    // rather than every entry matching either word.
    catalog.setQuery(QStringLiteral("dark grid"));
    QCOMPARE(catalog.resultCount(), 0);

    catalog.setQuery(QStringLiteral("   "));
    QVERIFY(!catalog.searching());
}

void SettingsCatalogTest::ranksTitleMatchesAboveDescriptionMatches()
{
    SettingsCatalog catalog;
    bool dark = true;
    bool other = false;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));

    SettingsCatalog::Entry titled = toggleEntry(QStringLiteral("appearance.dark"),
                                                QStringLiteral("appearance"),
                                                QStringLiteral("Dark appearance"), &dark);
    catalog.registerEntry(titled);

    SettingsCatalog::Entry described = toggleEntry(QStringLiteral("appearance.other"),
                                                   QStringLiteral("appearance"),
                                                   QStringLiteral("Panel tint"), &other);
    described.description = QStringLiteral("Tints panels when dark mode is active.");
    catalog.registerEntry(described);

    catalog.setQuery(QStringLiteral("dark"));
    QCOMPARE(catalog.resultCount(), 2);
    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("appearance.dark"));
}

void SettingsCatalogTest::ranksAvailableControlsAboveUnavailableOnes()
{
    SettingsCatalog catalog;
    bool present = true;
    bool missing = false;

    catalog.registerSection(QStringLiteral("sound"), QStringLiteral("Sound"));

    SettingsCatalog::Entry works = toggleEntry(QStringLiteral("sound.mute"),
                                               QStringLiteral("sound"),
                                               QStringLiteral("Mute output"), &present);
    catalog.registerEntry(works);

    SettingsCatalog::Entry broken = toggleEntry(QStringLiteral("sound.muteinput"),
                                                QStringLiteral("sound"),
                                                QStringLiteral("Mute input"), &missing);
    broken.available = []() { return false; };
    broken.unavailableReason = []() { return QStringLiteral("No capture device"); };
    catalog.registerEntry(broken);

    catalog.setQuery(QStringLiteral("mute"));
    QCOMPARE(catalog.resultCount(), 2);
    const QVariantList results = catalog.entries();
    QCOMPARE(results.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("sound.mute"));

    const QVariantMap unavailable = results.last().toMap();
    QVERIFY(!unavailable.value(QStringLiteral("available")).toBool());
    QCOMPARE(unavailable.value(QStringLiteral("unavailableReason")).toString(),
             QStringLiteral("No capture device"));
}

void SettingsCatalogTest::findsSettingsByKeywordAndSectionName()
{
    SettingsCatalog catalog;
    bool disturb = false;

    catalog.registerSection(QStringLiteral("notifications"), QStringLiteral("Notifications"));

    SettingsCatalog::Entry entry = toggleEntry(QStringLiteral("notifications.dnd"),
                                               QStringLiteral("notifications"),
                                               QStringLiteral("Do Not Disturb"), &disturb);
    entry.keywords = QStringList{QStringLiteral("quiet"), QStringLiteral("silence"),
                                 QStringLiteral("dnd")};
    catalog.registerEntry(entry);

    // A word the user is likely to reach for, but which is not in the title.
    catalog.setQuery(QStringLiteral("quiet"));
    QCOMPARE(catalog.resultCount(), 1);

    catalog.setQuery(QStringLiteral("dnd"));
    QCOMPARE(catalog.resultCount(), 1);

    // The section name is searchable too.
    catalog.setQuery(QStringLiteral("notifications"));
    QCOMPARE(catalog.resultCount(), 1);

    catalog.setQuery(QStringLiteral("bluetooth"));
    QCOMPARE(catalog.resultCount(), 0);
}

void SettingsCatalogTest::writesTogglesThroughTheBackingController()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;

    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    catalog.registerEntry(toggleEntry(QStringLiteral("appearance.dark"),
                                      QStringLiteral("appearance"),
                                      QStringLiteral("Dark appearance"), &desktop.darkMode));

    QSignalSpy results(&catalog, &SettingsCatalog::resultsChanged);
    QVERIFY(catalog.setValue(QStringLiteral("appearance.dark"), false));
    QVERIFY(!desktop.darkMode);
    QVERIFY(results.count() > 0);
    QVERIFY(!catalog.statusIsError());

    // The catalog reports the controller's current value, not a cached copy.
    QCOMPARE(catalog.entryFor(QStringLiteral("appearance.dark"))
                 .value(QStringLiteral("value")).toBool(), false);

    desktop.darkMode = true;
    QCOMPARE(catalog.entryFor(QStringLiteral("appearance.dark"))
                 .value(QStringLiteral("value")).toBool(), true);

    QVERIFY(!catalog.setValue(QStringLiteral("appearance.missing"), true));
    QVERIFY(catalog.statusIsError());
}

void SettingsCatalogTest::clampsSliderValuesToTheDeclaredRange()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;

    catalog.registerSection(QStringLiteral("sound"), QStringLiteral("Sound"));

    SettingsCatalog::Entry volume;
    volume.id = QStringLiteral("sound.volume");
    volume.section = QStringLiteral("sound");
    volume.title = QStringLiteral("Output volume");
    volume.kind = SettingsCatalog::sliderKind();
    volume.minimum = 0;
    volume.maximum = 100;
    volume.unit = QStringLiteral("%");
    volume.read = [&desktop]() { return QVariant(desktop.volume); };
    volume.write = [&desktop](const QVariant &value) {
        desktop.volume = value.toInt();
        return true;
    };
    QVERIFY(catalog.registerEntry(volume));

    QVERIFY(catalog.setValue(QStringLiteral("sound.volume"), 55));
    QCOMPARE(desktop.volume, 55);

    QVERIFY(catalog.setValue(QStringLiteral("sound.volume"), 250));
    QCOMPARE(desktop.volume, 100);

    QVERIFY(catalog.setValue(QStringLiteral("sound.volume"), -40));
    QCOMPARE(desktop.volume, 0);

    QVERIFY(!catalog.setValue(QStringLiteral("sound.volume"), QStringLiteral("loud")));
    QCOMPARE(desktop.volume, 0);
    QVERIFY(catalog.statusIsError());
}

void SettingsCatalogTest::refusesToWriteAnUnavailableControlAndStatesWhy()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;
    desktop.soundAvailable = false;

    catalog.registerSection(QStringLiteral("sound"), QStringLiteral("Sound"));

    SettingsCatalog::Entry volume;
    volume.id = QStringLiteral("sound.volume");
    volume.section = QStringLiteral("sound");
    volume.title = QStringLiteral("Output volume");
    volume.kind = SettingsCatalog::sliderKind();
    volume.read = [&desktop]() { return QVariant(desktop.volume); };
    volume.write = [&desktop](const QVariant &value) {
        desktop.volume = value.toInt();
        return true;
    };
    volume.available = [&desktop]() { return desktop.soundAvailable; };
    volume.unavailableReason = []() { return QStringLiteral("No mixer device available"); };
    QVERIFY(catalog.registerEntry(volume));

    const int before = desktop.volume;
    QVERIFY(!catalog.setValue(QStringLiteral("sound.volume"), 90));
    QCOMPARE(desktop.volume, before);
    QVERIFY(catalog.statusIsError());
    QVERIFY(catalog.statusMessage().contains(QStringLiteral("No mixer device available")));

    // When the capability appears, the same control starts working.
    desktop.soundAvailable = true;
    QVERIFY(catalog.setValue(QStringLiteral("sound.volume"), 90));
    QCOMPARE(desktop.volume, 90);
    QVERIFY(!catalog.statusIsError());
}

void SettingsCatalogTest::refusesToWriteReadOnlyEntries()
{
    SettingsCatalog catalog;
    catalog.registerSection(QStringLiteral("session"), QStringLiteral("Session"));
    catalog.registerEntry(infoEntry(QStringLiteral("session.display"), QStringLiteral("session"),
                                    QStringLiteral("Wayland display"),
                                    QStringLiteral("wayland-1")));

    QVERIFY(!catalog.setValue(QStringLiteral("session.display"), QStringLiteral("wayland-2")));
    QVERIFY(catalog.statusIsError());
    QVERIFY(!catalog.invoke(QStringLiteral("session.display")));

    const QVariantMap entry = catalog.entryFor(QStringLiteral("session.display"));
    QVERIFY(!entry.value(QStringLiteral("writable")).toBool());
    QCOMPARE(entry.value(QStringLiteral("value")).toString(), QStringLiteral("wayland-1"));
}

void SettingsCatalogTest::refusesChoicesWithNothingToChooseFrom()
{
    SettingsCatalog catalog;
    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    QString fit = QStringLiteral("fill");

    // A choice with no options presents a control the user cannot answer.
    SettingsCatalog::Entry empty;
    empty.id = QStringLiteral("appearance.emptychoice");
    empty.section = QStringLiteral("appearance");
    empty.title = QStringLiteral("Empty choice");
    empty.kind = SettingsCatalog::choiceKind();
    empty.read = [&fit]() { return QVariant(fit); };
    empty.write = [&fit](const QVariant &value) {
        fit = value.toString();
        return true;
    };
    QVERIFY(!catalog.registerEntry(empty));

    // An option with no value cannot be written back through the accessor.
    SettingsCatalog::Entry valueless = empty;
    valueless.id = QStringLiteral("appearance.valuelesschoice");
    valueless.options.append(SettingsCatalog::choiceOption(QString(), QStringLiteral("Nothing")));
    QVERIFY(!catalog.registerEntry(valueless));

    SettingsCatalog::Entry good = empty;
    good.id = QStringLiteral("appearance.fit");
    good.options.append(SettingsCatalog::choiceOption(QStringLiteral("fill"),
                                                     QStringLiteral("Fill screen")));
    good.options.append(SettingsCatalog::choiceOption(QStringLiteral("tile"),
                                                     QStringLiteral("Tile")));
    QVERIFY(catalog.registerEntry(good));
    QVERIFY(catalog.entryFor(QStringLiteral("appearance.fit"))
                .value(QStringLiteral("writable"))
                .toBool());
}

void SettingsCatalogTest::writesOnlyValuesAChoiceOffers()
{
    SettingsCatalog catalog;
    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    QString fit = QStringLiteral("fill");
    int writes = 0;

    SettingsCatalog::Entry entry;
    entry.id = QStringLiteral("appearance.fit");
    entry.section = QStringLiteral("appearance");
    entry.title = QStringLiteral("Background fit");
    entry.kind = SettingsCatalog::choiceKind();
    entry.options.append(SettingsCatalog::choiceOption(QStringLiteral("fill"),
                                                      QStringLiteral("Fill screen")));
    entry.options.append(SettingsCatalog::choiceOption(QStringLiteral("tile"),
                                                      QStringLiteral("Tile")));
    entry.read = [&fit]() { return QVariant(fit); };
    entry.write = [&fit, &writes](const QVariant &value) {
        ++writes;
        fit = value.toString();
        return true;
    };
    QVERIFY(catalog.registerEntry(entry));

    QVERIFY(catalog.setValue(QStringLiteral("appearance.fit"), QStringLiteral("tile")));
    QCOMPARE(fit, QStringLiteral("tile"));
    QCOMPARE(writes, 1);

    // A value not on the list never reaches the controller at all.
    QVERIFY(!catalog.setValue(QStringLiteral("appearance.fit"), QStringLiteral("diagonal")));
    QCOMPARE(fit, QStringLiteral("tile"));
    QCOMPARE(writes, 1);
    QVERIFY(catalog.statusIsError());
}

void SettingsCatalogTest::writesPathsAndReportsTheControllersReason()
{
    SettingsCatalog catalog;
    catalog.registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"));
    QString picture;
    bool accept = true;

    SettingsCatalog::Entry entry;
    entry.id = QStringLiteral("appearance.wallpaper");
    entry.section = QStringLiteral("appearance");
    entry.title = QStringLiteral("Desktop background");
    entry.kind = SettingsCatalog::pathKind();
    entry.emptyLabel = QStringLiteral("Built-in background");
    entry.read = [&picture]() { return QVariant(picture); };
    entry.write = [&picture, &accept](const QVariant &value) {
        if (!accept) {
            return false;
        }
        picture = value.toString();
        return true;
    };
    entry.writeFailureReason = []() {
        return QStringLiteral("photo.txt is not a picture Northstar can display.");
    };
    QVERIFY(catalog.registerEntry(entry));

    const QVariantMap described = catalog.entryFor(QStringLiteral("appearance.wallpaper"));
    QVERIFY(described.value(QStringLiteral("writable")).toBool());
    QCOMPARE(described.value(QStringLiteral("emptyLabel")).toString(),
             QStringLiteral("Built-in background"));

    QVERIFY(catalog.setValue(QStringLiteral("appearance.wallpaper"),
                             QStringLiteral("  /home/northstar/desert.png  ")));
    // The surface trims what it was given; judging the file is the
    // controller's business, not the catalog's.
    QCOMPARE(picture, QStringLiteral("/home/northstar/desert.png"));

    // A refusal repeats the controller's reason rather than inventing a
    // vaguer one of its own.
    accept = false;
    QVERIFY(!catalog.setValue(QStringLiteral("appearance.wallpaper"),
                              QStringLiteral("/home/northstar/photo.txt")));
    QVERIFY(catalog.statusIsError());
    QCOMPARE(catalog.statusMessage(),
             QStringLiteral("photo.txt is not a picture Northstar can display."));
}

void SettingsCatalogTest::performsActions()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;

    catalog.registerSection(QStringLiteral("desktop"), QStringLiteral("Desktop"));

    SettingsCatalog::Entry reset;
    reset.id = QStringLiteral("desktop.reset");
    reset.section = QStringLiteral("desktop");
    reset.title = QStringLiteral("Reset desktop icon layout");
    reset.actionLabel = QStringLiteral("Reset Layout");
    reset.kind = SettingsCatalog::actionKind();
    reset.destructive = true;
    reset.perform = [&desktop]() {
        ++desktop.resetCount;
        return desktop.resetSucceeds;
    };
    QVERIFY(catalog.registerEntry(reset));

    QVERIFY(catalog.invoke(QStringLiteral("desktop.reset")));
    QCOMPARE(desktop.resetCount, 1);
    QVERIFY(!catalog.statusIsError());

    const QVariantMap entry = catalog.entryFor(QStringLiteral("desktop.reset"));
    QVERIFY(entry.value(QStringLiteral("destructive")).toBool());
    QCOMPARE(entry.value(QStringLiteral("actionLabel")).toString(),
             QStringLiteral("Reset Layout"));

    QVERIFY(!catalog.invoke(QStringLiteral("desktop.absent")));
    QVERIFY(catalog.statusIsError());
}

void SettingsCatalogTest::reportsAFailedAction()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;
    desktop.resetSucceeds = false;

    catalog.registerSection(QStringLiteral("desktop"), QStringLiteral("Desktop"));

    SettingsCatalog::Entry reset;
    reset.id = QStringLiteral("desktop.reset");
    reset.section = QStringLiteral("desktop");
    reset.title = QStringLiteral("Reset desktop icon layout");
    reset.kind = SettingsCatalog::actionKind();
    reset.perform = [&desktop]() {
        ++desktop.resetCount;
        return desktop.resetSucceeds;
    };
    catalog.registerEntry(reset);

    QVERIFY(!catalog.invoke(QStringLiteral("desktop.reset")));
    QCOMPARE(desktop.resetCount, 1);
    QVERIFY(catalog.statusIsError());
    QVERIFY(catalog.statusMessage().contains(QStringLiteral("did not complete")));
}

void SettingsCatalogTest::reportsCurrentValuesOnEveryRead()
{
    SettingsCatalog catalog;
    FakeDesktop desktop;

    catalog.registerSection(QStringLiteral("sound"), QStringLiteral("Sound"));

    SettingsCatalog::Entry volume;
    volume.id = QStringLiteral("sound.volume");
    volume.section = QStringLiteral("sound");
    volume.title = QStringLiteral("Output volume");
    volume.kind = SettingsCatalog::sliderKind();
    volume.read = [&desktop]() { return QVariant(desktop.volume); };
    volume.write = [&desktop](const QVariant &value) {
        desktop.volume = value.toInt();
        return true;
    };
    catalog.registerEntry(volume);

    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("value")).toInt(), 40);

    // Something else moved the volume; Settings must show the new value
    // without being told what changed.
    desktop.volume = 12;
    QCOMPARE(catalog.entries().first().toMap().value(QStringLiteral("value")).toInt(), 12);

    QSignalSpy results(&catalog, &SettingsCatalog::resultsChanged);
    catalog.refresh();
    QCOMPARE(results.count(), 1);
}

QTEST_MAIN(SettingsCatalogTest)
#include "test-settingscatalog.moc"
