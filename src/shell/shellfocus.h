#pragma once

#include <QObject>
#include <QPointer>

class QWindow;

// Lets the QML shell ask for keyboard focus to return to the panel.
//
// The panel is the only shell surface configured to accept keyboard focus, so
// it is the only one that can keep the shell's application shortcuts alive
// once a transient surface closes.
class ShellFocus final : public QObject
{
    Q_OBJECT

public:
    explicit ShellFocus(QObject *parent = nullptr);

    void setPanelWindow(QWindow *window);

    Q_INVOKABLE bool restore();

private:
    QPointer<QWindow> m_panel;
};
