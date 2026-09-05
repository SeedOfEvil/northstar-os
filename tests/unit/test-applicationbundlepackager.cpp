#include "applicationbundlepackager.h"
#include "applicationbundlecatalog.h"
#include "applicationbundleinstaller.h"
#include "webapplication.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {
bool write(const QString &path, const QByteArray &bytes, bool executable = false)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size()
        && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                               | (executable ? QFileDevice::ExeOwner : QFileDevice::Permissions{}));
}
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QJsonObject recipe()
{
    return QJsonDocument::fromJson(R"({
        "schemaVersion":1, "bundleIdentifier":"org.northstar.PackagerTest",
        "displayName":"Packaging & Test", "version":"1.2.3", "executable":"program",
        "icon":"icon.svg", "categories":["Utility"],
        "license":{"id":"BSD-2-Clause", "file":"LICENSE"},
        "provenance":{"source":"local-test", "package":"packager-test", "revision":"fixture"}
    })").object();
}
bool fixture(const QString &root)
{
    return write(root + "/recipe.json", QJsonDocument(recipe()).toJson())
        && write(root + "/program", "#!/bin/sh\nprintf 'packaging-test-ok\\n'\n", true)
        && write(root + "/icon.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\"/>")
        && write(root + "/LICENSE", "Fixture licence text\n");
}
}

class TestApplicationBundlePackager : public QObject
{
    Q_OBJECT
private slots:
    void webBundleRoundTrip()
    {
        QTemporaryDir temp;
        QVERIFY(fixture(temp.path()));
        QJsonObject web = recipe();
        web.remove("executable");
        web.insert("schemaVersion", 2);
        const QString url = "https://example.org/?q=%24%28touch%20never%29&x=1";
        web.insert("web", QJsonObject{{"URL", url}, {"Browser", "firefox"}, {"Network", "required"},
                                     {"Storage", "shared-browser-profile"}, {"Permissions", "browser-managed"}});
        QVERIFY(write(temp.filePath("recipe.json"), QJsonDocument(web).toJson()));
        QString error;
        const QString bundlePath = temp.filePath("Web.app");
        QVERIFY2(ApplicationBundlePackager::package(temp.filePath("recipe.json"), bundlePath, &error), qPrintable(error));
        QVERIFY(QDir(bundlePath + "/Contents/Executable").isEmpty());
        QVERIFY(!read(bundlePath + "/Contents/Info.plist").contains("<key>Executable</key>"));
        BundleApplication bundle;
        QVERIFY(ApplicationBundleCatalog::inspectBundle(bundlePath, &bundle));
        QCOMPARE(bundle.webUrl, url);
        QVERIFY(bundle.executablePath.isEmpty());
        ApplicationBundleInstaller installer(temp.filePath("installed"), temp.filePath("trash"));
        const QVariantMap details = installer.bundleDetails(bundlePath);
        QCOMPARE(details.value("webOrigin").toString(), QString("https://example.org"));
        QVERIFY(details.value("webNotice").toString().contains("Shares Firefox cookies"));
        QVERIFY(installer.installBundle(bundlePath));
        ApplicationBundleCatalog catalog({temp.filePath("installed")});
        QCOMPARE(catalog.applications().first().toMap().value("genericName").toString(), QString("Web app • Firefox"));
        QString program;
        QStringList arguments;
        QVERIFY(catalog.launchSpec(bundle.desktopId, &program, &arguments));
        QCOMPARE(program, QString("/usr/local/bin/firefox"));
        QCOMPARE(arguments, QStringList({"--new-window", url}));
        // No actual browser process or network connection is needed for the test.
        const QString installedManifest = temp.filePath("installed/") + bundle.bundleId + ".app/Contents/Info.plist";
        QByteArray changed = read(installedManifest);
        changed.replace("https://example.org/", "file:///tmp/");
        QVERIFY(write(installedManifest, changed));
        QVERIFY(!catalog.launchSpec(bundle.desktopId, &program, &arguments));
        // Restore before testing the existing validated Trash workflow.
        QVERIFY(write(installedManifest, read(bundlePath + "/Contents/Info.plist")));
        QVERIFY(installer.removeBundle(bundle.bundleId));
        const QByteArray original = read(bundlePath + "/Contents/Info.plist");
        for (const QByteArray &extra : {QByteArray("<key>Executable</key><string>app</string>"),
                                       QByteArray("<key>DocumentExtensions</key><array><string>txt</string></array>")}) {
            QByteArray mixed = original;
            mixed.replace("<key>Icon</key>", extra + "<key>Icon</key>");
            QVERIFY(write(bundlePath + "/Contents/Info.plist", mixed));
            QVERIFY(!ApplicationBundleCatalog::inspectBundle(bundlePath, &bundle));
        }
        QVERIFY(write(bundlePath + "/Contents/Info.plist", original));
        QVERIFY(write(bundlePath + "/Contents/Executable/.hidden-script", "#!/bin/sh\nexit 0\n", true));
        QVERIFY(!ApplicationBundleCatalog::inspectBundle(bundlePath, &bundle));
    }

    void unsafeWebRecipes_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("value");
        QTest::newRow("http") << QString("URL") << QString("http://example.org/");
        QTest::newRow("javascript") << QString("URL") << QString("javascript:alert(1)");
        QTest::newRow("file") << QString("URL") << QString("file:///etc/passwd");
        QTest::newRow("data") << QString("URL") << QString("data:text/html,hello");
        QTest::newRow("relative") << QString("URL") << QString("//example.org/");
        QTest::newRow("flags") << QString("URL") << QString("--profile /tmp/profile");
        QTest::newRow("credentials") << QString("URL") << QString("https://user:password@example.org/");
        QTest::newRow("hostless") << QString("URL") << QString("https:/path");
        QTest::newRow("newline") << QString("URL") << QString("https://example.org/\n");
        QTest::newRow("backslash") << QString("URL") << QString("https://example.org\\evil");
        QTest::newRow("invalid-percent") << QString("URL") << QString("https://example.org/%zz");
        QTest::newRow("browser-path") << QString("Browser") << QString("/tmp/firefox");
        QTest::newRow("offline-claim") << QString("Network") << QString("offline");
        QTest::newRow("isolation-claim") << QString("Storage") << QString("isolated");
        QTest::newRow("permissions-claim") << QString("Permissions") << QString("none");
        QTest::newRow("extra-field") << QString("Arguments") << QString("--no-remote");
    }
    void unsafeWebRecipes()
    {
        QFETCH(QString, key);
        QFETCH(QString, value);
        QTemporaryDir temp;
        QVERIFY(fixture(temp.path()));
        QJsonObject web = recipe();
        web.remove("executable");
        web.insert("schemaVersion", 2);
        QJsonObject fields{{"URL", "https://example.org/"}, {"Browser", "firefox"}, {"Network", "required"},
                           {"Storage", "shared-browser-profile"}, {"Permissions", "browser-managed"}};
        fields.insert(key, value);
        web.insert("web", fields);
        QVERIFY(write(temp.filePath("recipe.json"), QJsonDocument(web).toJson()));
        QString error;
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("Bad.app"), &error));
        QVERIFY(!QFileInfo::exists(temp.filePath("Bad.app")));
    }

    void roundTripAndDeterminism()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString root = temp.path();
        QVERIFY(fixture(root));
        QString error;
        const QString first = root + "/First.app";
        const QString second = root + "/Second.app";
        QVERIFY2(ApplicationBundlePackager::package(root + "/recipe.json", first, &error), qPrintable(error));
        QVERIFY2(ApplicationBundlePackager::package(root + "/recipe.json", second, &error), qPrintable(error));
        for (const QString &relative : {QString("Contents/Info.plist"), QString("Contents/Executable/app"),
                                        QString("Contents/Resources/icon.svg"), QString("Contents/Resources/LICENSE")}) {
            QCOMPARE(read(first + '/' + relative), read(second + '/' + relative));
            QVERIFY(!(QFileInfo(first + '/' + relative).permissions()
                      & (QFileDevice::WriteGroup | QFileDevice::WriteOther)));
        }
        QVERIFY(read(first + "/Contents/Info.plist").contains("BSD-2-Clause"));
        QCOMPARE(read(first + "/Contents/Resources/LICENSE"), read(root + "/LICENSE"));
        BundleApplication bundle;
        QVERIFY(ApplicationBundleCatalog::inspectBundle(first, &bundle));
        QCOMPARE(bundle.name, QString("Packaging & Test"));
        ApplicationBundleInstaller installer(root + "/installed", root + "/trash");
        QVERIFY(installer.installBundle(first));
        ApplicationBundleCatalog catalog({root + "/installed"});
        QString program;
        QStringList arguments;
        QVERIFY(catalog.launchSpec("bundle:org.northstar.PackagerTest", &program, &arguments));
        QProcess process;
        process.start(program, arguments);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        QCOMPARE(process.readAllStandardOutput(), QByteArray("packaging-test-ok\n"));
        QVERIFY(!installer.installBundle(first));
        QVERIFY(installer.removeBundle(bundle.bundleId));
        QVERIFY(QFileInfo::exists(root + "/trash/files/org.northstar.PackagerTest.app"));
        const QByteArray original = read(first + "/Contents/Info.plist");
        QVERIFY(!ApplicationBundlePackager::package(root + "/recipe.json", first, &error));
        QCOMPARE(read(first + "/Contents/Info.plist"), original);
    }

    void badRecipes_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("schema") << QString("schemaVersion") << QJsonValue(2);
        QTest::newRow("unknown-field") << QString("buildHook") << QJsonValue("evil");
        QTest::newRow("identifier") << QString("bundleIdentifier") << QJsonValue("../outside");
        QTest::newRow("name-type") << QString("displayName") << QJsonValue(3);
        QTest::newRow("name-control") << QString("displayName") << QJsonValue("Name\nBad");
        QTest::newRow("version") << QString("version") << QJsonValue("latest");
        QTest::newRow("categories") << QString("categories") << QJsonValue("Utility");
        QTest::newRow("provenance") << QString("provenance") << QJsonValue(QJsonObject{});
        QTest::newRow("licence") << QString("license") << QJsonValue(QJsonObject{});
        QTest::newRow("traversal") << QString("executable") << QJsonValue("../program");
        QTest::newRow("absolute") << QString("executable") << QJsonValue("/bin/sh");
        QTest::newRow("missing") << QString("executable") << QJsonValue("missing");
    }
    void badRecipes()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QTemporaryDir temp;
        QVERIFY(fixture(temp.path()));
        QJsonObject bad = recipe();
        bad.insert(field, value);
        QVERIFY(write(temp.filePath("recipe.json"), QJsonDocument(bad).toJson()));
        QString error;
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("Bad.app"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(temp.filePath("Bad.app")));
        QCOMPARE(QDir(temp.path()).entryList({".northstar-package-*"}, QDir::Dirs | QDir::Hidden).size(), 0);
    }

    void unsafeInputs()
    {
        QTemporaryDir temp;
        const QString root = temp.path();
        QString error;
        const auto rejected = [&] { return !ApplicationBundlePackager::package(root + "/recipe.json", root + "/Bad.app", &error); };
        QVERIFY(fixture(root));
        QVERIFY(write(root + "/program", "MZ-not-a-native-program", true));
        QVERIFY(rejected());
        QVERIFY(fixture(root));
        QVERIFY(QFile::setPermissions(root + "/program", QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(rejected());
        QVERIFY(fixture(root));
        QVERIFY(QFile::setPermissions(root + "/program", QFileDevice::ReadOwner | QFileDevice::ExeOwner | QFileDevice::WriteGroup));
        QVERIFY(rejected());
        QVERIFY(QFile::setPermissions(root + "/program", QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(fixture(root));
        QVERIFY(QFile::remove(root + "/program"));
        QVERIFY(QFile::link(root + "/LICENSE", root + "/program"));
        QVERIFY(rejected());
        QVERIFY(QFile::remove(root + "/program"));
#ifdef Q_OS_UNIX
        QCOMPARE(::mkfifo(QFile::encodeName(root + "/program").constData(), 0700), 0);
        QVERIFY(rejected());
        QVERIFY(QFile::remove(root + "/program"));
#endif
        QVERIFY(fixture(root));
        QVERIFY(write(root + "/icon.svg", "<svg><broken></svg>"));
        QVERIFY(rejected());
        QVERIFY(fixture(root));
        QVERIFY(write(root + "/LICENSE", "  \n"));
        QVERIFY(rejected());
        QVERIFY(write(root + "/LICENSE", QByteArray::fromHex("fffe")));
        QVERIFY(rejected());
        QVERIFY(fixture(root));
        QFile large(root + "/program");
        QVERIFY(large.open(QIODevice::WriteOnly));
        QVERIFY(large.resize(128LL * 1024 * 1024 + 1));
        large.close();
        QVERIFY(rejected());
        QVERIFY(!QFileInfo::exists(root + "/Bad.app"));
    }

    void invalidDestinationAndRecipe()
    {
        QTemporaryDir temp;
        QVERIFY(fixture(temp.path()));
        QString error;
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("not-a-bundle"), &error));
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("missing/Bad.app"), &error));
        QVERIFY(QDir().mkdir(temp.filePath("Bad.app")));
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("Bad.app"), &error));
        QVERIFY(write(temp.filePath("recipe.json"), "{malformed}"));
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("New.app"), &error));
        QVERIFY(fixture(temp.path()));
        QVERIFY(QFile::rename(temp.filePath("recipe.json"), temp.filePath("real.json")));
        QVERIFY(QFile::link(temp.filePath("real.json"), temp.filePath("recipe.json")));
        QVERIFY(!ApplicationBundlePackager::package(temp.filePath("recipe.json"), temp.filePath("New.app"), &error));
        QVERIFY(!QFileInfo::exists(temp.filePath("New.app")));
    }
};
QTEST_GUILESS_MAIN(TestApplicationBundlePackager)
#include "test-applicationbundlepackager.moc"
