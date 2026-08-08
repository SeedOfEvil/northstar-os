#include "applicationbundlecatalog.h"
#include "applicationlauncher.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
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
                    const QByteArray &icon = "northstar-welcome.svg")
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<plist version=\"1.0\"><dict>\n"
           "<key>BundleIdentifier</key><string>" + bundleId + "</string>\n"
           "<key>DisplayName</key><string>Northstar Welcome</string>\n"
           "<key>Version</key><string>0.1.0</string>\n"
           "<key>Executable</key><string>" + executable + "</string>\n"
           "<key>Icon</key><string>" + icon + "</string>\n"
           "<key>Categories</key><array><string>Utility</string></array>\n"
           "</dict></plist>\n";
}

bool createBundle(const QString &directory,
                  const QByteArray &bundleId,
                  const QByteArray &executable = "northstar-welcome",
                  const QByteArray &icon = "northstar-welcome.svg")
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
                     manifest(bundleId, executable, icon),
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
    void rejectsMalformedTraversalAndWritableBundles();
    void launcherMergesBundleApplicationsAndPassesFiles();
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

    const QVariantMap item = catalog.applications().first().toMap();
    QCOMPARE(item.value(QStringLiteral("sourceType")).toString(), QStringLiteral("bundle"));
    QCOMPARE(item.value(QStringLiteral("genericName")).toString(), QStringLiteral("Version 0.1.0"));
    QVERIFY(item.value(QStringLiteral("iconSource")).toUrl().isLocalFile());

    QString program;
    QStringList arguments;
    QVERIFY(catalog.launchSpec(QStringLiteral("bundle:org.northstar.Welcome"), &program, &arguments));
    QCOMPARE(program, QFileInfo(catalog.entries().first().executablePath).canonicalFilePath());
    QVERIFY(arguments.isEmpty());
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

QTEST_MAIN(ApplicationBundleCatalogTest)
#include "test-applicationbundlecatalog.moc"
