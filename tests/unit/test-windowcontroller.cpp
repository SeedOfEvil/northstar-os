#include "windowcontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QtTest>

class WindowControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void refreshFiltersDesktopAndShellViews();
    void actionsUseWayfireViewIds();
};

namespace {

QJsonObject view(int id, qint64 pid, const QString &title, bool minimized = false)
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("title"), title},
        {QStringLiteral("app-id"), title.toLower()},
        {QStringLiteral("mapped"), true},
        {QStringLiteral("role"), QStringLiteral("TOPLEVEL")},
        {QStringLiteral("minimized"), minimized},
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
                view(4, 99123, QStringLiteral("Terminal")),
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

QTEST_MAIN(WindowControllerTest)
#include "test-windowcontroller.moc"
