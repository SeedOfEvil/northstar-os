#include "pinnedapplicationmodel.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class PinnedApplicationModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAndRolesAreStable();
    void mutationsPersistInOrder();
    void rejectsInvalidAndDuplicateMutations();
};

void PinnedApplicationModelTest::defaultsAndRolesAreStable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PinnedApplicationModel model(nullptr, directory.filePath(QStringLiteral("preferences.ini")));

    QCOMPARE(model.desktopIds(), QStringList({QStringLiteral("qterminal"), QStringLiteral("firefox")}));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), PinnedApplicationModel::DesktopIdRole).toString(),
             QStringLiteral("qterminal"));
    QCOMPARE(model.roleNames().value(PinnedApplicationModel::DesktopIdRole), QByteArrayLiteral("desktopId"));
}

void PinnedApplicationModelTest::mutationsPersistInOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("preferences.ini"));

    {
        PinnedApplicationModel model(nullptr, path);
        QSignalSpy changed(&model, &PinnedApplicationModel::desktopIdsChanged);
        QVERIFY(model.pin(QStringLiteral("org.northstar.TextEditor")));
        QVERIFY(model.movePinned(2, 0));
        QVERIFY(model.unpin(QStringLiteral("firefox")));
        QCOMPARE(changed.count(), 3);
        QCOMPARE(model.desktopIds(), QStringList({
            QStringLiteral("org.northstar.TextEditor"),
            QStringLiteral("qterminal"),
        }));
    }

    PinnedApplicationModel restored(nullptr, path);
    QCOMPARE(restored.desktopIds(), QStringList({
        QStringLiteral("org.northstar.TextEditor"),
        QStringLiteral("qterminal"),
    }));
}

void PinnedApplicationModelTest::rejectsInvalidAndDuplicateMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PinnedApplicationModel model(nullptr, directory.filePath(QStringLiteral("preferences.ini")));

    QVERIFY(!model.pin(QStringLiteral("  ")));
    QVERIFY(!model.pin(QStringLiteral("qterminal")));
    QVERIFY(!model.movePinned(-1, 0));
    QVERIFY(!model.movePinned(0, 9));
    QVERIFY(!model.unpin(QStringLiteral("missing")));
    QCOMPARE(model.rowCount(), 2);
}

QTEST_MAIN(PinnedApplicationModelTest)
#include "test-pinnedapplicationmodel.moc"
