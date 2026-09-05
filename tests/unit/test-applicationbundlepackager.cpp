#include "applicationbundlepackager.h"
#include "applicationbundlecatalog.h"
#include "applicationbundleinstaller.h"

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
