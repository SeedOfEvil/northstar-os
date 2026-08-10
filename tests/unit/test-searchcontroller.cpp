#include "applicationlauncher.h"
#include "searchcontroller.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

int resultIndex(const QVariantList &results, const QString &kind, const QString &title)
{
    for (int index = 0; index < results.size(); ++index) {
        const QVariantMap result = results.at(index).toMap();
        if (result.value(QStringLiteral("kind")).toString() == kind
            && result.value(QStringLiteral("title")).toString() == title) {
            return index;
        }
    }
    return -1;
}

} // namespace

class SearchControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsBoundedActionsAndApplications();
    void searchesHomeAsynchronouslyAndActivatesSafely();
    void debouncesSupersededQueries();
};

void SearchControllerTest::returnsBoundedActionsAndApplications()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString applicationsPath = directory.filePath(QStringLiteral("applications"));
    QVERIFY(QDir().mkpath(applicationsPath));
    writeFile(QDir(applicationsPath).filePath(QStringLiteral("sample-notes.desktop")),
              QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Sample Notes\n"
                                "Exec=/bin/echo\nIcon=accessories-text-editor\n"
                                "Categories=Utility;\nMimeType=text/plain;\n"));

    ApplicationLauncher launcher(nullptr, {}, {applicationsPath},
                                 directory.filePath(QStringLiteral("launch.log")));
    SearchController controller(&launcher, nullptr, directory.path());

    controller.setQuery(QStringLiteral("settings"));
    const int settingsIndex = resultIndex(controller.results(), QStringLiteral("action"),
                                          QStringLiteral("Settings"));
    QVERIFY(settingsIndex >= 0);
    const QVariantMap settings = controller.results().at(settingsIndex).toMap();
    QCOMPARE(settings.value(QStringLiteral("category")).toString(), QStringLiteral("Actions"));
    QCOMPARE(settings.value(QStringLiteral("activationData")).toString(), QStringLiteral("settings"));

    QSignalSpy actionRequested(&controller, &SearchController::actionRequested);
    QVERIFY(controller.activateResult(settingsIndex));
    QCOMPARE(actionRequested.count(), 1);
    QCOMPARE(actionRequested.first().first().toString(), QStringLiteral("settings"));

    controller.setQuery(QStringLiteral("sample notes"));
    const int applicationIndex = resultIndex(controller.results(), QStringLiteral("application"),
                                             QStringLiteral("Sample Notes"));
    QVERIFY(applicationIndex >= 0);
    QSignalSpy applicationRequested(&controller, &SearchController::applicationRequested);
    QVERIFY(controller.activateResult(applicationIndex));
    QCOMPARE(applicationRequested.first().first().toString(), QStringLiteral("sample-notes"));
    QVERIFY(!controller.activateResult(999));
}

void SearchControllerTest::searchesHomeAsynchronouslyAndActivatesSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("Documents"))));
    writeFile(directory.filePath(QStringLiteral("Documents/needle-notes.txt")),
              QByteArrayLiteral("Northstar search test\n"));
    writeFile(directory.filePath(QStringLiteral(".hidden-needle.txt")), QByteArrayLiteral("hidden\n"));

    SearchController controller(nullptr, nullptr, directory.path());
    controller.setQuery(QStringLiteral("needle"));
    QVERIFY(controller.searching());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.searching(), 5000);

    const int fileIndex = resultIndex(controller.results(), QStringLiteral("file"),
                                      QStringLiteral("needle-notes.txt"));
    QVERIFY(fileIndex >= 0);
    QCOMPARE(resultIndex(controller.results(), QStringLiteral("file"),
                         QStringLiteral(".hidden-needle.txt")), -1);

    QSignalSpy fileRequested(&controller, &SearchController::fileRequested);
    QVERIFY(controller.activateResult(fileIndex));
    QCOMPARE(fileRequested.count(), 1);
    QCOMPARE(fileRequested.first().at(0).toString(),
             QFileInfo(directory.filePath(QStringLiteral("Documents/needle-notes.txt"))).canonicalFilePath());
    QCOMPARE(fileRequested.first().at(1).toBool(), false);
}

void SearchControllerTest::debouncesSupersededQueries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeFile(directory.filePath(QStringLiteral("first-target.txt")), QByteArrayLiteral("first\n"));
    writeFile(directory.filePath(QStringLiteral("second-target.txt")), QByteArrayLiteral("second\n"));

    SearchController controller(nullptr, nullptr, directory.path());
    controller.setQuery(QStringLiteral("first"));
    controller.setQuery(QStringLiteral("second"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.searching(), 5000);

    QVERIFY(resultIndex(controller.results(), QStringLiteral("file"),
                        QStringLiteral("second-target.txt")) >= 0);
    QCOMPARE(resultIndex(controller.results(), QStringLiteral("file"),
                         QStringLiteral("first-target.txt")), -1);
}

QTEST_MAIN(SearchControllerTest)
#include "test-searchcontroller.moc"
