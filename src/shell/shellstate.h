#pragma once

#include <QObject>
#include <QStringList>

class ShellState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList pinnedApplications READ pinnedApplications CONSTANT)
    Q_PROPERTY(QString activeWindowTitle READ activeWindowTitle WRITE setActiveWindowTitle NOTIFY activeWindowTitleChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit ShellState(QObject *parent = nullptr, QString settingsPath = {});

    QStringList pinnedApplications() const;
    QString activeWindowTitle() const;
    bool darkMode() const;

public slots:
    void setActiveWindowTitle(const QString &title);
    void setDarkMode(bool enabled);
    void toggleDarkMode();

signals:
    void activeWindowTitleChanged();
    void darkModeChanged();

private:
    void loadPreferences();
    void savePreferences() const;

    const QStringList m_pinnedApplications;
    QString m_settingsPath;
    QString m_activeWindowTitle;
    bool m_darkMode = true;
};
