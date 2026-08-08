#include "applicationcatalog.h"
#include "applicationlauncher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QString applicationsDirectory(const QTemporaryDir &temporaryDirectory, const QString &name)
{
    const QString path = QDir(temporaryDirectory.path()).filePath(name);
    QDir().mkpath(path);
    return path;
}

void writeDesktopFile(const QString &directory, const QString &desktopId, const QByteArray &contents)
{
    QFile file(QDir(directory).filePath(desktopId + QStringLiteral(".desktop")));
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.fileName()));
    QCOMPARE(file.write(contents), contents.size());
}

QByteArray applicationEntry(const QByteArray &name, const QByteArray &exec)
{
    return "[Desktop Entry]\n"
           "Type=Application\n"
           "Name=" + name + "\n"
           "Exec=" + exec + "\n"
           "Categories=Utility;\n";
}

} // namespace

class ApplicationCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndSortsDesktopEntries();
    void honorsDirectoryPrecedenceAndRefreshes();
    void searchesByNameCategoryAndDesktopId();
    void expandsDesktopExecWithoutShellEvaluation();
    void launcherUsesCatalogArguments();
};

void ApplicationCatalogTest::filtersAndSortsDesktopEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString applications = applicationsDirectory(temporaryDirectory, QStringLiteral("applications"));

    writeDesktopFile(applications, QStringLiteral("zulu"), applicationEntry("Zulu", "zulu"));
    writeDesktopFile(applications, QStringLiteral("alpha"), applicationEntry("Alpha", "alpha"));
    writeDesktopFile(applications, QStringLiteral("hidden"),
                     "[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nHidden=true\n");
    writeDesktopFile(applications, QStringLiteral("nodisplay"),
                     "[Desktop Entry]\nType=Application\nName=No display\nExec=nodisplay\nNoDisplay=true\n");
    writeDesktopFile(applications, QStringLiteral("service"),
                     "[Desktop Entry]\nType=Service\nName=Service\nExec=service\n");
    writeDesktopFile(applications, QStringLiteral("other-desktop"),
                     "[Desktop Entry]\nType=Application\nName=Other desktop\nExec=other\nOnlyShowIn=GNOME;\n");
    writeDesktopFile(applications, QStringLiteral("missing-field"),
                     "[Desktop Entry]\nType=Application\nName=Missing field\nExec=missing %x\n");

    ApplicationCatalog catalog({applications});

    QCOMPARE(catalog.applicationIds(), QStringList({QStringLiteral("alpha"), QStringLiteral("zulu")}));
    QCOMPARE(catalog.entries().first().name, QStringLiteral("Alpha"));
    QCOMPARE(catalog.applications().first().toMap().value(QStringLiteral("desktopId")).toString(), QStringLiteral("alpha"));
}

void ApplicationCatalogTest::honorsDirectoryPrecedenceAndRefreshes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString userApplications = applicationsDirectory(temporaryDirectory, QStringLiteral("user/applications"));
    const QString systemApplications = applicationsDirectory(temporaryDirectory, QStringLiteral("system/applications"));

    writeDesktopFile(systemApplications, QStringLiteral("shared"), applicationEntry("System copy", "system-copy"));
    writeDesktopFile(userApplications, QStringLiteral("shared"), applicationEntry("User copy", "user-copy"));

    ApplicationCatalog catalog({userApplications, systemApplications});
    QSignalSpy changedSpy(&catalog, &ApplicationCatalog::applicationsChanged);

    QCOMPARE(catalog.entries().size(), 1);
    QCOMPARE(catalog.entries().first().name, QStringLiteral("User copy"));
    QVERIFY(!catalog.reload());
    QCOMPARE(changedSpy.count(), 0);

    writeDesktopFile(userApplications, QStringLiteral("shared"), applicationEntry("Updated copy", "updated-copy"));
    QVERIFY(catalog.reload());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(catalog.entries().first().name, QStringLiteral("Updated copy"));
}

void ApplicationCatalogTest::searchesByNameCategoryAndDesktopId()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString applications = applicationsDirectory(temporaryDirectory, QStringLiteral("applications"));

    writeDesktopFile(applications, QStringLiteral("web-browser"), applicationEntry("Aurora Browser", "aurora"));
    writeDesktopFile(applications, QStringLiteral("notes"), applicationEntry("Notes", "notes"));

    ApplicationCatalog catalog({applications});

    const QVariantList browserMatches = catalog.searchApplications(QStringLiteral("browser"));
    QCOMPARE(browserMatches.size(), 1);
    QCOMPARE(browserMatches.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Aurora Browser"));

    QCOMPARE(catalog.searchApplications(QStringLiteral("utility notes")).size(), 1);
    QCOMPARE(catalog.searchApplications(QStringLiteral("web-browser")).size(), 1);
    QVERIFY(catalog.searchApplications(QStringLiteral("does-not-exist")).isEmpty());
}

void ApplicationCatalogTest::expandsDesktopExecWithoutShellEvaluation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString applications = applicationsDirectory(temporaryDirectory, QStringLiteral("applications"));
    const QString desktopFile = QDir(applications).filePath(QStringLiteral("demo.desktop"));

    writeDesktopFile(applications, QStringLiteral("demo"),
                     "[Desktop Entry]\n"
                     "Type=Application\n"
                     "Name=Demo Tool\n"
                     "Icon=demo-icon\n"
                     "Exec=demo --label \"Hello world\" %i %c %k %U %%\n");

    ApplicationCatalog catalog({applications});
    QString program;
    QStringList arguments;
    QVERIFY(catalog.launchSpec(QStringLiteral("demo"), &program, &arguments));
    QCOMPARE(program, QStringLiteral("demo"));
    QCOMPARE(arguments, QStringList({
        QStringLiteral("--label"),
        QStringLiteral("Hello world"),
        QStringLiteral("--icon"),
        QStringLiteral("demo-icon"),
        QStringLiteral("Demo Tool"),
        QFileInfo(desktopFile).absoluteFilePath(),
        QStringLiteral("%")
    }));

    QVERIFY(!catalog.launchSpec(QStringLiteral("does-not-exist"), &program, &arguments));
}

void ApplicationCatalogTest::launcherUsesCatalogArguments()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString applications = applicationsDirectory(temporaryDirectory, QStringLiteral("applications"));
    writeDesktopFile(applications, QStringLiteral("demo"), applicationEntry("Demo", "demo --ready %f"));

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
        {applications},
        temporaryDirectory.filePath(QStringLiteral("launch.log")));

    QSignalSpy matchingSpy(&launcher, &ApplicationLauncher::matchingApplicationsChanged);
    launcher.setApplicationQuery(QStringLiteral("demo"));
    QCOMPARE(launcher.matchingApplications().size(), 1);
    QCOMPARE(matchingSpy.count(), 1);

    QVERIFY(launcher.launchApplication(QStringLiteral("demo")));
    QCOMPARE(launchedProgram, QStringLiteral("demo"));
    QCOMPARE(launchedArguments, QStringList({QStringLiteral("--ready")}));
    QCOMPARE(launcher.lastLaunchDesktopId(), QStringLiteral("demo"));
    QCOMPARE(launcher.lastLaunchPid(), 4321);
    QVERIFY(launcher.lastLaunchSucceeded());

    const QString documentPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("notes.txt"));
    QFile document(documentPath);
    QVERIFY(document.open(QIODevice::WriteOnly));
    document.write("Northstar");
    document.close();

    QVERIFY(launcher.launchApplicationWithFile(QStringLiteral("demo"), documentPath));
    QCOMPARE(launchedProgram, QStringLiteral("demo"));
    QCOMPARE(launchedArguments, QStringList({
        QStringLiteral("--ready"),
        QFileInfo(documentPath).absoluteFilePath()
    }));

    QVERIFY(!launcher.launchApplication(QStringLiteral("missing")));
    QCOMPARE(launcher.lastLaunchDesktopId(), QStringLiteral("missing"));
    QCOMPARE(launcher.lastLaunchPid(), 0);
    QVERIFY(!launcher.lastLaunchSucceeded());
    QVERIFY(launcher.launchMessage().contains(QStringLiteral("missing")));
}

QTEST_MAIN(ApplicationCatalogTest)
#include "test-applicationcatalog.moc"
