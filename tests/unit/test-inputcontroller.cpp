#include "inputcontroller.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class InputControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void detectsWayfirePointersAndPersistsControls();
    void reportsUnavailableInventoryHonestly();
};

void InputControllerTest::detectsWayfirePointersAndPersistsControls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString config = directory.filePath(QStringLiteral("wayfire.ini"));
    QFile initial(config);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write("[core]\nplugins = ipc command\n\n[input]\ntap_to_click = false\n\n[output:eDP-1]\nmode = auto\n");
    initial.close();

    const auto request = [](const QString &method, const QJsonObject &data,
                            QJsonValue *response, QString *) {
        if (method != QStringLiteral("input/list-devices") || !data.isEmpty()) {
            return false;
        }
        *response = QJsonArray{
            QJsonObject{{QStringLiteral("name"), QStringLiteral("USB Mouse")},
                        {QStringLiteral("type"), QStringLiteral("pointer")},
                        {QStringLiteral("enabled"), true}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("DELL TouchPad")},
                        {QStringLiteral("type"), QStringLiteral("pointer")},
                        {QStringLiteral("enabled"), true}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("Keyboard")},
                        {QStringLiteral("type"), QStringLiteral("keyboard")},
                        {QStringLiteral("enabled"), true}}};
        return true;
    };

    InputController controller(nullptr, config, request);
    QVERIFY(controller.available());
    QVERIFY(controller.mouseAvailable());
    QVERIFY(controller.touchpadAvailable());
    QCOMPARE(controller.deviceNames().size(), 2);
    QVERIFY(!controller.tapToClick());

    QVERIFY(controller.setTapToClick(true));
    QVERIFY(controller.setDisableWhileTyping(true));
    QVERIFY(controller.setMouseNaturalScroll(true));
    QVERIFY(controller.setTouchpadNaturalScroll(true));
    QVERIFY(controller.setMouseSpeed(-35));
    QVERIFY(controller.setTouchpadSpeed(45));
    QVERIFY(controller.setClickMethod(QStringLiteral("clickfinger")));
    QVERIFY(!controller.setClickMethod(QStringLiteral("unsupported")));
    QVERIFY(!controller.setMouseSpeed(101));

    QFile saved(config);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const QByteArray text = saved.readAll();
    QVERIFY(text.contains("plugins = ipc command"));
    QVERIFY(text.contains("mode = auto"));
    QCOMPARE(text.count("tap_to_click = "), 1);
    QVERIFY(text.contains("tap_to_click = true"));
    QVERIFY(text.contains("disable_touchpad_while_typing = true"));
    QVERIFY(text.contains("mouse_cursor_speed = -0.35"));
    QVERIFY(text.contains("touchpad_cursor_speed = 0.45"));
    QVERIFY(text.contains("click_method = clickfinger"));
}

void InputControllerTest::reportsUnavailableInventoryHonestly()
{
    QTemporaryDir directory;
    InputController controller(nullptr, directory.filePath(QStringLiteral("wayfire.ini")),
        [](const QString &, const QJsonObject &, QJsonValue *, QString *error) {
            *error = QStringLiteral("Wayfire input service is unavailable");
            return false;
        });
    QVERIFY(!controller.available());
    QVERIFY(!controller.mouseAvailable());
    QVERIFY(!controller.touchpadAvailable());
    QCOMPARE(controller.statusMessage(), QStringLiteral("Wayfire input service is unavailable"));
}

QTEST_MAIN(InputControllerTest)
#include "test-inputcontroller.moc"
