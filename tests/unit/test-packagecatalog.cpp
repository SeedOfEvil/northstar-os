#include "packagecatalog.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class PackageCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void separatesRequestedPackagesFromDependencies();
    void readsUpdateAvailabilityAndOrphansFromVersionOutput();
    void ignoresVersionLinesThatSayNothingUseful();
    void parsesAndSortsPackageQueryOutput();
    void ignoresMalformedAndDuplicateRows();
    void filtersAcrossPackageFields();
    void repeatedRefreshNotifiesCompletion();
};

void PackageCatalogTest::parsesAndSortsPackageQueryOutput()
{
    const QList<InstalledPackage> packages = PackageCatalog::parseQueryOutput(
        QByteArray("zlib|1.3|Compression library\n"
                   "bash|5.2|Bourne shell\n"));

    QCOMPARE(packages.size(), 2);
    QCOMPARE(packages.at(0).name, QStringLiteral("bash"));
    QCOMPARE(packages.at(0).version, QStringLiteral("5.2"));
    QCOMPARE(packages.at(1).name, QStringLiteral("zlib"));
}

void PackageCatalogTest::ignoresMalformedAndDuplicateRows()
{
    const QList<InstalledPackage> packages = PackageCatalog::parseQueryOutput(
        QByteArray("invalid\n"
                   "|1.0|missing name\n"
                   "qt6||missing version\n"
                   "qt6|6.8|first|with a pipe\n"
                   "qt6|6.8|duplicate\n"));

    QCOMPARE(packages.size(), 1);
    QCOMPARE(packages.constFirst().comment, QStringLiteral("first|with a pipe"));
}

void PackageCatalogTest::filtersAcrossPackageFields()
{
    const QList<InstalledPackage> packages{
        {QStringLiteral("qt6-qtbase"), QStringLiteral("6.8.2"), QStringLiteral("Qt application framework")},
        {QStringLiteral("qterminal"), QStringLiteral("1.4"), QStringLiteral("Terminal emulator")},
    };

    QCOMPARE(PackageCatalog::filterPackages(packages, QStringLiteral("qt framework")).size(), 1);
    QCOMPARE(PackageCatalog::filterPackages(packages, QStringLiteral("TERMINAL")).constFirst().name,
             QStringLiteral("qterminal"));
    QCOMPARE(PackageCatalog::filterPackages(packages, QString()).size(), 2);
}

void PackageCatalogTest::repeatedRefreshNotifiesCompletion()
{
    const QString echoPath = QStandardPaths::findExecutable(QStringLiteral("echo"));
    if (echoPath.isEmpty()) {
        QSKIP("echo is required for the package refresh signal test");
    }

    PackageCatalog catalog(echoPath);
    QSignalSpy refreshingSpy(&catalog, &PackageCatalog::refreshingChanged);

    QVERIFY(catalog.refresh());
    QVERIFY(!catalog.refreshing());
    QCOMPARE(refreshingSpy.count(), 2);

    QVERIFY(catalog.refresh());
    QVERIFY(!catalog.refreshing());
    QCOMPARE(refreshingSpy.count(), 4);
}


void PackageCatalogTest::separatesRequestedPackagesFromDependencies()
{
    // The fourth field is pkg's automatic flag: 1 means the package arrived
    // as a dependency rather than because anyone asked for it.
    const QByteArray output =
        "firefox|153.0.1|Web browser|0\n"
        "libXfont2|2.0.8|X font library|1\n"
        "perl5|5.42.2|Practical Extraction and Report Language|1\n"
        "qterminal|1.4.0|Terminal emulator|0\n";

    const QList<InstalledPackage> packages = PackageCatalog::parseQueryOutput(output);
    QCOMPARE(packages.size(), 4);

    int requested = 0;
    for (const InstalledPackage &package : packages) {
        if (!package.automatic) {
            ++requested;
        }
    }
    QCOMPARE(requested, 2);

    // A line written before the flag existed must not vanish from the list;
    // treating the absence as "requested" keeps it visible.
    const QList<InstalledPackage> older = PackageCatalog::parseQueryOutput(
        "firefox|153.0.1|Web browser\n");
    QCOMPARE(older.size(), 1);
    QVERIFY(!older.constFirst().automatic);

    // A comment may contain the separator, so the flag is recognised only
    // when the trailing field is exactly the flag it claims to be. Splitting
    // the line on every separator would have taken "does a" as the comment.
    const QList<InstalledPackage> awkward =
        PackageCatalog::parseQueryOutput("tool|1.0|does a|b|c thing|1\n");
    QCOMPARE(awkward.size(), 1);
    QCOMPARE(awkward.constFirst().comment, QStringLiteral("does a|b|c thing"));
    QVERIFY(awkward.constFirst().automatic);
}

void PackageCatalogTest::readsUpdateAvailabilityAndOrphansFromVersionOutput()
{
    // Real output from the validation machine, including the orphaned
    // Northstar package whose origin has left the ports tree.
    const QByteArray output =
        "firefox-153.0.1,2                  <   needs updating (remote has 153.0.3,2)\n"
        "libXfont2-2.0.8                    <   needs updating (remote has 2.0.9)\n"
        "northstar-0.1.4                    ?   orphaned: x11/northstar\n"
        "perl5-5.42.2                       <   needs updating (remote has 5.42.3)\n";

    const QList<InstalledPackage> scanned = PackageCatalog::parseVersionOutput(output);
    QCOMPARE(scanned.size(), 4);

    QCOMPARE(scanned.at(0).name, QStringLiteral("firefox"));
    QCOMPARE(scanned.at(0).version, QStringLiteral("153.0.1,2"));
    QVERIFY(scanned.at(0).updatable);
    QCOMPARE(scanned.at(0).availableVersion, QStringLiteral("153.0.3,2"));
    QVERIFY(!scanned.at(0).orphaned);

    // An orphan is not updatable: there is nothing to update it to, and
    // showing it as current would be the wrong answer too.
    const InstalledPackage &orphan = scanned.at(2);
    QCOMPARE(orphan.name, QStringLiteral("northstar"));
    QVERIFY(orphan.orphaned);
    QVERIFY(!orphan.updatable);
    QVERIFY(orphan.availableVersion.isEmpty());
}

void PackageCatalogTest::ignoresVersionLinesThatSayNothingUseful()
{
    const QByteArray output =
        "uptodate-1.0                       =   up-to-date with remote\n"
        "newer-2.0                          >   succeeds remote (remote has 1.0)\n"
        "malformed\n"
        "\n"
        "-1.0                               <   needs updating (remote has 2.0)\n";

    // A package that is current, or ahead of the remote, is not an update.
    // A line with no name is not a package at all.
    QVERIFY(PackageCatalog::parseVersionOutput(output).isEmpty());
}

QTEST_MAIN(PackageCatalogTest)

#include "test-packagecatalog.moc"
