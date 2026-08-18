#pragma once

#include <QString>
#include <QStringList>

// User-private, on-disk history of recently opened documents.
//
// The store is deliberately small and path-only: it records where the user has
// been, never document contents. The backing file is written with owner-only
// permissions so a shared machine does not leak one account's document names.
class RecentFilesStore final
{
public:
    explicit RecentFilesStore(QString settingsPath = {});

    static int maximumEntries();

    QString settingsPath() const;
    QStringList paths() const;

    void remember(const QString &path);
    bool forget(const QString &path);
    void clear();

private:
    void load();
    bool store() const;

    QString m_settingsPath;
    QStringList m_paths;
};
