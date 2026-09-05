#include "applicationbundlecatalog.h"
#include "applicationbundleinstaller.h"
#include "applicationlauncher.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QList>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest/QtTest>

namespace {

QString bundleDirectory(const QTemporaryDir &temporaryDirectory)
{
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("apps"));
    QDir().mkpath(path);
    return path;
}

QString bundlePath(const QString &directory, const QString &name)
{
    return QDir(directory).filePath(name + QStringLiteral(".app"));
}

bool writeFile(const QString &path, const QByteArray &contents, QFileDevice::Permissions permissions)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(contents) != contents.size()) {
        return false;
    }
    file.close();
    return QFile::setPermissions(path, permissions);
}

QByteArray manifest(const QByteArray &bundleId,
                    const QByteArray &executable = "northstar-welcome",
                    const QByteArray &icon = "northstar-welcome.svg",
                    bool includeProvenance = true,
                    const QList<QByteArray> &documentExtensions = {})
{
    QByteArray result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<plist version=\"1.0\"><dict>\n"
           "<key>BundleIdentifier</key><string>" + bundleId + "</string>\n"
           "<key>DisplayName</key><string>Northstar Welcome</string>\n"
           "<key>Version</key><string>0.1.0</string>\n"
           "<key>Executable</key><string>" + executable + "</string>\n"
           "<key>Icon</key><string>" + icon + "</string>\n"
           "<key>Categories</key><array><string>Utility</string></array>\n"
           ;
    if (!documentExtensions.isEmpty()) {
        result += "<key>DocumentExtensions</key><array>";
        for (const QByteArray &extension : documentExtensions) {
            result += "<string>" + extension + "</string>";
        }
        result += "</array>\n";
    }
    if (includeProvenance) {
        result += "<key>Provenance</key><dict>\n"
                  "<key>Source</key><string>northstar-project</string>\n"
                  "<key>Package</key><string>northstar-welcome</string>\n"
                  "<key>Revision</key><string>development</string>\n"
                  "</dict>\n";
    }
    result += "</dict></plist>\n";
    return result;
}

bool createBundle(const QString &directory,
                  const QByteArray &bundleId,
                  const QByteArray &executable = "northstar-welcome",
                  const QByteArray &icon = "northstar-welcome.svg",
                  bool includeProvenance = true,
                  const QList<QByteArray> &documentExtensions = {})
{
    const QString root = bundlePath(directory, QString::fromUtf8(bundleId));
    const QString contents = QDir(root).filePath(QStringLiteral("Contents"));
    const QString executableRoot = QDir(contents).filePath(QStringLiteral("Executable"));
    const QString resourcesRoot = QDir(contents).filePath(QStringLiteral("Resources"));
    if (!QDir().mkpath(executableRoot) || !QDir().mkpath(resourcesRoot)) {
        return false;
    }

    const QFileDevice::Permissions regularFilePermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    const QFileDevice::Permissions executablePermissions = regularFilePermissions | QFileDevice::ExeOwner;
    return writeFile(QDir(contents).filePath(QStringLiteral("Info.plist")),
                     manifest(bundleId, executable, icon, includeProvenance, documentExtensions),
                     regularFilePermissions)
        && writeFile(QDir(executableRoot).filePath(QString::fromUtf8(executable)),
                     "#!/bin/sh\nexit 0\n",
                     executablePermissions)
        && writeFile(QDir(resourcesRoot).filePath(QString::fromUtf8(icon)),
                     "<svg xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"8\" height=\"8\"/></svg>\n",
                     regularFilePermissions);
}

} // namespace

class ApplicationBundleCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void discoversValidBundleAndLaunchSpec();
    void autoRefreshesBundleDirectories();
    void rejectsMalformedTraversalAndWritableBundles();
    void launcherMergesBundleApplicationsAndPassesFiles();
    void filtersApplicationsByDeclaredDocumentType();
    void installsAndMovesUserBundleToTrash();
    void rejectsDuplicateAndUnsafePayload();
    void reportsProvenanceAndRejectsSystemIdentifier();
    void marksOnlyDefaultUserBundlesAsUserInstalled();
    void launcherForwardsBundleWorkflow();
    void webLauncherNeverPassesLocalFiles();
};

void ApplicationBundleCatalogTest::discoversValidBundleAndLaunchSpec()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString apps = bundleDirectory(temporaryDirectory);
    QVERIFY(createBundle(apps, "org.northstar.Welcome"));

    ApplicationBundleCatalog catalog({apps});
    QCOMPARE(catalog.applicationIds(), QStringList({QStringLiteral("bundle:org.northstar.Welcome")}));
    QCOMPARE(catalog.entries().first().name, QStringLiteral("Northstar Welcome"));
    QCOMPARE(catalog.entries().first().categories, QStringList({QStringLiteral("Utility")}));
    QCOMPARE(catalog.entries().first().provenance.source, QStringLiteral("northstar-project"));
    QCOMPARE(catalog.entries().first().provenance.package, QStringLiteral("northstar-welcome"));
    QCOMPARE(catalog.entries().first().provenance.revision, QStringLiteral("development"));

    const QVariantMap item = catalog.applications().first().toMap();
    QCOMPARE(item.value(QStringLiteral("sourceType")).toString(), QStringLiteral("bundle"));
    QCOMPARE(item.value(QStringLiteral("genericName")).toString(), QStringLiteral("Version 0.1.0"));
    QCOMPARE(item.value(QStringLiteral("provenanceSource")).toString(), QStringLiteral("northstar-project"));
    QCOMPARE(item.value(QStringLiteral("provenancePackage")).toString(), QStringLiteral("northstar-welcome"));
    QCOMPARE(item.value(QStringLiteral("provenanceRevision")).toString(), QStringLiteral("development"));
    QVERIFY(item.value(QStringLiteral("iconSource")).toUrl().isLocalFile());

    QString program;
    QStringList arguments;
    QVERIFY(catalog.launchSpec(QStringLiteral("bundle:org.northstar.Welcome"), &program, &arguments));
    QCOMPARE(program, QFileInfo(catalog.entries().first().executablePath).canonicalFilePath());
    QVERIFY(arguments.isEmpty());
}

void ApplicationBundleCatalogTest::autoRefreshesBundleDirectories()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString apps = bundleDirectory(temporaryDirectory);
    ApplicationBundleCatalog catalog({apps});
    QSignalSpy changedSpy(&catalog, &ApplicationBundleCatalog::applicationsChanged);

    QVERIFY(createBundle(apps, "org.northstar.Live"));

    QTRY_VERIFY_WITH_TIMEOUT(
        catalog.applicationIds().contains(QStringLiteral("bundle:org.northstar.Live")), 3000);
    QVERIFY(changedSpy.count() >= 1);
}

void ApplicationBundleCatalogTest::rejectsMalformedTraversalAndWritableBundles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString apps = bundleDirectory(temporaryDirectory);

    QVERIFY(createBundle(apps, "org.northstar.Traversal"));
    QVERIFY(createBundle(apps, "org.northstar.Absolute"));
    QVERIFY(createBundle(apps, "org.northstar.Writable"));
    QVERIFY(createBundle(apps, "org.northstar.Malformed"));
    QVERIFY(createBundle(apps, "org.northstar.MissingProvenance"));
    QVERIFY(createBundle(apps, "org.northstar.InvalidProvenance"));

    const QString traversalManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.Traversal")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QVERIFY(writeFile(traversalManifest,
                      manifest("org.northstar.Traversal", "../escape"),
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    const QString absoluteManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.Absolute")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QVERIFY(writeFile(absoluteManifest,
                      manifest("org.northstar.Absolute", "/bin/sh"),
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    const QString malformedManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.Malformed")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QVERIFY(writeFile(malformedManifest, "<plist><dict>", QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    const QString writableManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.Writable")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QVERIFY(writeFile(writableManifest,
                      manifest("org.northstar.Writable"),
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    const QString writableExecutable = QDir(bundlePath(apps, QStringLiteral("org.northstar.Writable")))
        .filePath(QStringLiteral("Contents/Executable/northstar-welcome"));
    QVERIFY(writeFile(writableExecutable,
                      "#!/bin/sh\nexit 0\n",
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner
                          | QFileDevice::ExeOwner | QFileDevice::WriteOther));

    const QString missingProvenanceManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.MissingProvenance")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QVERIFY(writeFile(missingProvenanceManifest,
                      manifest("org.northstar.MissingProvenance", "northstar-welcome", "northstar-welcome.svg", false),
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    const QString invalidProvenanceManifest = QDir(bundlePath(apps, QStringLiteral("org.northstar.InvalidProvenance")))
        .filePath(QStringLiteral("Contents/Info.plist"));
    QByteArray invalidProvenance = manifest("org.northstar.InvalidProvenance");
    invalidProvenance.replace("<key>Source</key><string>northstar-project</string>",
                              "<key>Source</key><string> northstar-project </string>");
    QVERIFY(writeFile(invalidProvenanceManifest,
                      invalidProvenance,
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    ApplicationBundleCatalog catalog({apps});
    QVERIFY(catalog.entries().isEmpty());
}

void ApplicationBundleCatalogTest::launcherMergesBundleApplicationsAndPassesFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString desktopDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("desktop"));
    const QString apps = bundleDirectory(temporaryDirectory);
    QVERIFY(QDir().mkpath(desktopDirectory));
    QVERIFY(createBundle(apps, "org.northstar.Welcome"));

    QString launchedProgram;
    QStringList launchedArguments;
    ApplicationLauncher launcher(
        nullptr,
        [&launchedProgram, &launchedArguments](const QString &program, const QStringList &arguments, qint64 *pid) {
            launchedProgram = program;
            launchedArguments = arguments;
            if (pid != nullptr) {
                *pid = 4321;
            }
            return true;
        },
        {desktopDirectory},
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("launch.log")),
        {apps});

    QCOMPARE(launcher.applications().size(), 1);
    launcher.setApplicationQuery(QStringLiteral("welcome"));
    QCOMPARE(launcher.matchingApplications().size(), 1);
    QVERIFY(launcher.launchApplication(QStringLiteral("bundle:org.northstar.Welcome")));
    QCOMPARE(launchedProgram, QFileInfo(QDir(apps).filePath(
        QStringLiteral("org.northstar.Welcome.app/Contents/Executable/northstar-welcome"))).canonicalFilePath());
    QVERIFY(launchedArguments.isEmpty());

    const QString documentPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeFile(documentPath, "Northstar", QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(launcher.launchApplicationWithFile(QStringLiteral("bundle:org.northstar.Welcome"), documentPath));
    QCOMPARE(launchedArguments, QStringList({QFileInfo(documentPath).absoluteFilePath()}));
}

void ApplicationBundleCatalogTest::filtersApplicationsByDeclaredDocumentType()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString desktopDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("desktop"));
    const QString apps = bundleDirectory(temporaryDirectory);
    QVERIFY(QDir().mkpath(desktopDirectory));
    QVERIFY(createBundle(apps, "org.northstar.Welcome"));
    QVERIFY(createBundle(apps,
                         "org.northstar.TextEditor",
                         "northstar-text-editor",
                         "northstar-text-editor.svg",
                         true,
                         {"txt", "md"}));

    ApplicationLauncher launcher(
        nullptr,
        {},
        {desktopDirectory},
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("launch.log")),
        {apps},
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("associations.ini")));

    const QString documentPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeFile(documentPath, "Northstar", QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    const QVariantList compatible = launcher.applicationsForFile(documentPath);
    QCOMPARE(compatible.size(), 1);
    QCOMPARE(compatible.first().toMap().value(QStringLiteral("desktopId")).toString(),
             QStringLiteral("bundle:org.northstar.TextEditor"));

    const QString imagePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("image.png"));
    QVERIFY(writeFile(imagePath, "PNG", QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(launcher.applicationsForFile(imagePath).isEmpty());
}

void ApplicationBundleCatalogTest::installsAndMovesUserBundleToTrash()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("source"));
    const QString applicationRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("user-apps"));
    const QString trashRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Trash"));
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(createBundle(sourceRoot, "org.northstar.Welcome"));

    ApplicationBundleInstaller installer(applicationRoot, trashRoot);
    QVERIFY2(installer.installBundle(bundlePath(sourceRoot, QStringLiteral("org.northstar.Welcome"))),
             qPrintable(installer.statusMessage()));
    QVERIFY(!installer.error());
    QVERIFY(QFileInfo::exists(bundlePath(sourceRoot, QStringLiteral("org.northstar.Welcome"))));

    const QString installedPath = bundlePath(applicationRoot, QStringLiteral("org.northstar.Welcome"));
    BundleApplication installed;
    QVERIFY(ApplicationBundleCatalog::inspectBundle(installedPath, &installed));
    QCOMPARE(installed.bundleId, QStringLiteral("org.northstar.Welcome"));

    QVERIFY2(installer.removeBundle(QStringLiteral("org.northstar.Welcome")),
             qPrintable(installer.statusMessage()));
    QVERIFY(!QFileInfo::exists(installedPath));
    QVERIFY(QFileInfo::exists(
        QDir(trashRoot).filePath(QStringLiteral("files/org.northstar.Welcome.app"))));
    QVERIFY(QFileInfo::exists(
        QDir(trashRoot).filePath(QStringLiteral("info/org.northstar.Welcome.app.trashinfo"))));
}

void ApplicationBundleCatalogTest::rejectsDuplicateAndUnsafePayload()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("source"));
    const QString applicationRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("user-apps"));
    const QString trashRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Trash"));
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(createBundle(sourceRoot, "org.northstar.Valid"));
    QVERIFY(createBundle(sourceRoot, "org.northstar.Unsafe"));

    ApplicationBundleInstaller installer(applicationRoot, trashRoot);
    const QString validPath = bundlePath(sourceRoot, QStringLiteral("org.northstar.Valid"));
    QVERIFY2(installer.installBundle(validPath), qPrintable(installer.statusMessage()));
    QVERIFY(!installer.installBundle(validPath));
    QVERIFY(installer.error());

    const QString unsafePayload = QDir(bundlePath(sourceRoot, QStringLiteral("org.northstar.Unsafe")))
        .filePath(QStringLiteral("Contents/Resources/untrusted-data"));
    QVERIFY(writeFile(unsafePayload,
                      "unsafe\n",
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::WriteOther));
    const QVariantMap unsafeDetails = installer.bundleDetails(
        bundlePath(sourceRoot, QStringLiteral("org.northstar.Unsafe")));
    QCOMPARE(unsafeDetails.value(QStringLiteral("valid")).toBool(), false);
    QVERIFY(!unsafeDetails.value(QStringLiteral("validationError")).toString().isEmpty());
    QVERIFY(!installer.installBundle(bundlePath(sourceRoot, QStringLiteral("org.northstar.Unsafe"))));
    QVERIFY(installer.error());
    QVERIFY(!QFileInfo::exists(bundlePath(applicationRoot, QStringLiteral("org.northstar.Unsafe"))));
}

void ApplicationBundleCatalogTest::reportsProvenanceAndRejectsSystemIdentifier()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("source"));
    const QString systemRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("system"));
    const QString applicationRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("user-apps"));
    const QString trashRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Trash"));
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(QDir().mkpath(systemRoot));
    QVERIFY(createBundle(sourceRoot, "org.northstar.Conflict"));
    QVERIFY(createBundle(systemRoot, "org.northstar.Conflict"));

    ApplicationBundleInstaller installer(applicationRoot, trashRoot, nullptr, {systemRoot});
    const QString sourcePath = bundlePath(sourceRoot, QStringLiteral("org.northstar.Conflict"));
    const QVariantMap details = installer.bundleDetails(sourcePath);
    QCOMPARE(details.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(details.value(QStringLiteral("bundleIdentifier")).toString(),
             QStringLiteral("org.northstar.Conflict"));
    QCOMPARE(details.value(QStringLiteral("source")).toString(), QStringLiteral("northstar-project"));
    QCOMPARE(details.value(QStringLiteral("package")).toString(), QStringLiteral("northstar-welcome"));
    QCOMPARE(details.value(QStringLiteral("revision")).toString(), QStringLiteral("development"));
    QCOMPARE(details.value(QStringLiteral("alreadyInstalled")).toBool(), true);
    QCOMPARE(details.value(QStringLiteral("installedScope")).toString(), QStringLiteral("system"));

    QVERIFY(!installer.installBundle(sourcePath));
    QVERIFY(installer.error());
    QVERIFY(installer.statusMessage().contains(QStringLiteral("package-owned")));
    QVERIFY(!QFileInfo::exists(bundlePath(applicationRoot, QStringLiteral("org.northstar.Conflict"))));
}

void ApplicationBundleCatalogTest::marksOnlyDefaultUserBundlesAsUserInstalled()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", temporaryDirectory.path().toUtf8());
    const QString userRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("northstar/apps"));
    QVERIFY(QDir().mkpath(userRoot));
    QVERIFY(createBundle(userRoot, "org.northstar.UserApplication"));

    ApplicationBundleCatalog catalog({userRoot});
    QCOMPARE(catalog.applications().size(), 1);
    const QVariantMap application = catalog.applications().first().toMap();
    QCOMPARE(application.value(QStringLiteral("bundleIdentifier")).toString(),
             QStringLiteral("org.northstar.UserApplication"));
    QCOMPARE(application.value(QStringLiteral("userInstalled")).toBool(), true);

    if (previousDataHome.isNull()) {
        qunsetenv("XDG_DATA_HOME");
    } else {
        qputenv("XDG_DATA_HOME", previousDataHome);
    }
}

void ApplicationBundleCatalogTest::launcherForwardsBundleWorkflow()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("source"));
    const QString applicationRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("user-apps"));
    const QString trashRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Trash"));
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(createBundle(sourceRoot, "org.northstar.Forwarded"));

    ApplicationLauncher launcher(nullptr, {}, {}, {}, {applicationRoot});
    ApplicationBundleInstaller installer(applicationRoot, trashRoot);
    launcher.setApplicationBundleInstaller(&installer);
    const QString sourcePath = bundlePath(sourceRoot, QStringLiteral("org.northstar.Forwarded"));

    QCOMPARE(launcher.applicationBundleDetails(sourcePath).value(QStringLiteral("valid")).toBool(),
             true);
    QVERIFY(launcher.installApplicationBundle(sourcePath));
    QVERIFY(!launcher.applicationBundleError());
    QVERIFY(!launcher.applicationBundleStatusMessage().isEmpty());
    QCOMPARE(launcher.applicationBundleDetails(sourcePath)
                 .value(QStringLiteral("installedScope")).toString(),
             QStringLiteral("user"));
    QVERIFY(launcher.removeApplicationBundle(QStringLiteral("org.northstar.Forwarded")));
    QVERIFY(!launcher.applicationBundleError());
}

void ApplicationBundleCatalogTest::webLauncherNeverPassesLocalFiles()
{
    QTemporaryDir temp;
    QVERIFY(createBundle(temp.path(), "org.northstar.WebTest"));
    const QString bundle = bundlePath(temp.path(), "org.northstar.WebTest");
    QVERIFY(QFile::remove(bundle + "/Contents/Executable/northstar-welcome"));
    QByteArray xml = manifest("org.northstar.WebTest");
    xml.replace("<key>Executable</key><string>northstar-welcome</string>",
                "<key>WebApplication</key><dict><key>URL</key><string>https://example.org/</string>"
                "<key>Browser</key><string>firefox</string><key>Network</key><string>required</string>"
                "<key>Storage</key><string>shared-browser-profile</string>"
                "<key>Permissions</key><string>browser-managed</string></dict>");
    QVERIFY(writeFile(bundle + "/Contents/Info.plist", xml, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    int launches = 0;
    QVERIFY(QDir().mkdir(temp.filePath("desktop")));
    ApplicationLauncher launcher(nullptr, [&](const QString &program, const QStringList &args, qint64 *) {
        ++launches;
        return program == "/usr/local/bin/firefox" && args == QStringList({"--new-window", "https://example.org/"});
    }, {temp.filePath("desktop")}, temp.filePath("launch.log"), {temp.path()});
    QVERIFY(launcher.launchApplication("bundle:org.northstar.WebTest"));
    QCOMPARE(launches, 1);
    QVERIFY(!launcher.launchApplicationWithFile("bundle:org.northstar.WebTest", bundle + "/Contents/Info.plist"));
    QCOMPARE(launches, 1);
    QVERIFY(launcher.applicationsForFile(bundle + "/Contents/Info.plist").isEmpty());
    ApplicationLauncher unavailable(nullptr, [](const QString &, const QStringList &, qint64 *) { return false; },
                                    {temp.filePath("desktop")}, temp.filePath("failed.log"), {temp.path()});
    QVERIFY(!unavailable.launchApplication("bundle:org.northstar.WebTest"));
    QVERIFY(unavailable.launchMessage().contains("Check that Firefox is installed"));
}

QTEST_MAIN(ApplicationBundleCatalogTest)
#include "test-applicationbundlecatalog.moc"
