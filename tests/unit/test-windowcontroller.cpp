#include "windowcontroller.h"

#include <QCoreApplication>

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QtTest>

class WindowControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void refreshFiltersDesktopAndShellViews();
    void listsTheShellsOwnWindowsButNotItsPanels();
    void groupsWindowsByApplicationIdentity();
    void actionsUseWayfireViewIds();
};

namespace {

QJsonObject view(int id,
                 qint64 pid,
                 const QString &title,
                 bool minimized = false,
                 bool focused = false)
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("title"), title},
        {QStringLiteral("app-id"), title.toLower()},
        {QStringLiteral("mapped"), true},
        {QStringLiteral("role"), QStringLiteral("TOPLEVEL")},
        {QStringLiteral("minimized"), minimized},
        {QStringLiteral("focused"), focused},
    };
}

} // namespace

void WindowControllerTest::refreshFiltersDesktopAndShellViews()
{
    WindowController controller(
        nullptr,
        [](const QString &method, const QJsonObject &, QJsonValue *response, QString *) {
            if (method != QStringLiteral("window-rules/list-views")) {
                return false;
            }
            *response = QJsonArray{
                view(4, 99123, QStringLiteral("Terminal"), false, true),
                QJsonObject{
                    {QStringLiteral("id"), 5},
                    {QStringLiteral("pid"), 99124},
                    {QStringLiteral("mapped"), true},
                    {QStringLiteral("role"), QStringLiteral("desktop-environment")},
                },
                QJsonObject{
                    {QStringLiteral("id"), 6},
                    {QStringLiteral("pid"), 99125},
                    {QStringLiteral("mapped"), false},
                    {QStringLiteral("role"), QStringLiteral("TOPLEVEL")},
                },
            };
            return true;
        });

    QVERIFY(controller.refresh());
    QVERIFY(controller.available());
    QCOMPARE(controller.windows().size(), 1);
    QCOMPARE(controller.windows().first().toMap().value(QStringLiteral("viewId")).toInt(), 4);
    QCOMPARE(controller.windows().first().toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Terminal"));
    QVERIFY(controller.windows().first().toMap().value(QStringLiteral("active")).toBool());
}

void WindowControllerTest::listsTheShellsOwnWindowsButNotItsPanels()
{
    // Measured from the compositor with the Software Center open: the panel,
    // dock, and background report role desktop-environment, while the window
    // itself reports toplevel. All four belong to the shell process, so the
    // role is what separates them and the process cannot.
    const qint64 shellPid = QCoreApplication::applicationPid();

    WindowController controller(
        nullptr,
        [shellPid](const QString &method, const QJsonObject &, QJsonValue *response, QString *) {
            if (method != QStringLiteral("window-rules/list-views")) {
                return false;
            }
            *response = QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), 3},
                    {QStringLiteral("pid"), shellPid},
                    {QStringLiteral("app-id"), QStringLiteral("northstar-background-0")},
                    {QStringLiteral("title"), QStringLiteral("layer-shell")},
                    {QStringLiteral("mapped"), true},
                    {QStringLiteral("role"), QStringLiteral("desktop-environment")},
                },
                QJsonObject{
                    {QStringLiteral("id"), 5},
                    {QStringLiteral("pid"), shellPid},
                    {QStringLiteral("app-id"), QStringLiteral("northstar-dock-0")},
                    {QStringLiteral("title"), QStringLiteral("layer-shell")},
                    {QStringLiteral("mapped"), true},
                    {QStringLiteral("role"), QStringLiteral("desktop-environment")},
                },
                QJsonObject{
                    {QStringLiteral("id"), 8},
                    {QStringLiteral("pid"), shellPid},
                    {QStringLiteral("app-id"), QStringLiteral("northstar-shell")},
                    {QStringLiteral("title"), QStringLiteral("Northstar Software")},
                    {QStringLiteral("mapped"), true},
                    {QStringLiteral("role"), QStringLiteral("toplevel")},
                    {QStringLiteral("minimized"), true},
                },
            };
            return true;
        });

    QVERIFY(controller.refresh());

    // The one window a person can point at is listed; the surfaces that make
    // up the desktop itself are not. Without this a minimised Settings or
    // Software Center window had nowhere to be restored from.
    QCOMPARE(controller.windows().size(), 1);
    const QVariantMap window = controller.windows().constFirst().toMap();
    QCOMPARE(window.value(QStringLiteral("viewId")).toInt(), 8);
    QCOMPARE(window.value(QStringLiteral("title")).toString(),
             QStringLiteral("Northstar Software"));
    QVERIFY(window.value(QStringLiteral("minimized")).toBool());
}

void WindowControllerTest::actionsUseWayfireViewIds()
{
    QStringList methods;
    int focusedViewId = 0;
    int minimizedViewId = 0;
    bool minimized = false;
    WindowController controller(
        nullptr,
        [&methods, &focusedViewId, &minimizedViewId, &minimized](const QString &method,
                                                                  const QJsonObject &data,
                                                                  QJsonValue *response,
                                                                  QString *) {
            methods.append(method);
            if (method == QStringLiteral("window-rules/list-views")) {
                *response = QJsonArray{view(12, 99126, QStringLiteral("Firefox"), minimized)};
                return true;
            }
            if (method == QStringLiteral("window-rules/focus-view")) {
                focusedViewId = data.value(QStringLiteral("id")).toInt();
                *response = QJsonObject{};
                return true;
            }
            if (method == QStringLiteral("wm-actions/set-minimized")) {
                minimizedViewId = data.value(QStringLiteral("view_id")).toInt();
                minimized = data.value(QStringLiteral("state")).toBool();
                *response = QJsonObject{};
                return true;
            }
            return false;
        });

    QVERIFY(controller.refresh());
    QVERIFY(controller.activateWindow(12));
    QVERIFY(controller.toggleMinimize(12));
    QVERIFY(minimized);
    QCOMPARE(focusedViewId, 12);
    QCOMPARE(minimizedViewId, 12);
    QCOMPARE(methods, QStringList({
        QStringLiteral("window-rules/list-views"),
        QStringLiteral("window-rules/focus-view"),
        QStringLiteral("window-rules/list-views"),
        QStringLiteral("wm-actions/set-minimized"),
        QStringLiteral("window-rules/list-views"),
    }));
}

void WindowControllerTest::groupsWindowsByApplicationIdentity()
{
    WindowController controller(
        nullptr,
        [](const QString &method, const QJsonObject &, QJsonValue *response, QString *) {
            if (method != QStringLiteral("window-rules/list-views")) {
                return false;
            }
            QJsonObject first = view(21, 99131, QStringLiteral("Terminal one"), false, true);
            first.insert(QStringLiteral("app-id"), QStringLiteral("qterminal"));
            QJsonObject second = view(22, 99132, QStringLiteral("Terminal two"), true, false);
            second.insert(QStringLiteral("app-id"), QStringLiteral("qterminal.desktop"));
            QJsonObject browser = view(23, 99133, QStringLiteral("Firefox"), true, false);
            browser.insert(QStringLiteral("app-id"), QStringLiteral("firefox"));
            *response = QJsonArray{first, second, browser};
            return true;
        });

    QVERIFY(controller.refresh());
    QCOMPARE(controller.applicationGroups().size(), 2);

    const QVariantMap terminals = controller.applicationGroups().first().toMap();
    QCOMPARE(terminals.value(QStringLiteral("identity")).toString(), QStringLiteral("qterminal"));
    QCOMPARE(terminals.value(QStringLiteral("count")).toInt(), 2);
    QCOMPARE(terminals.value(QStringLiteral("windows")).toList().size(), 2);
    QVERIFY(terminals.value(QStringLiteral("active")).toBool());
    QVERIFY(!terminals.value(QStringLiteral("allMinimized")).toBool());

    const QVariantMap firefox = controller.applicationGroups().last().toMap();
    QCOMPARE(firefox.value(QStringLiteral("identity")).toString(), QStringLiteral("firefox"));
    QCOMPARE(firefox.value(QStringLiteral("count")).toInt(), 1);
    QVERIFY(firefox.value(QStringLiteral("allMinimized")).toBool());
}

QTEST_MAIN(WindowControllerTest)
#include "test-windowcontroller.moc"
