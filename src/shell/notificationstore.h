#pragma once

#include "notificationcenter.h"

#include <QList>
#include <QString>

// User-private, on-disk history for the notification centre.
//
// Notifications name what the user did and which applications they ran, so the
// backing file is written with owner-only permissions exactly like the text
// editor's recent-document history.
//
// The store is deliberately forgiving when reading: a truncated or hand-edited
// file must never stop the shell from starting, so malformed records are
// skipped rather than treated as an error.
class NotificationStore final
{
public:
    explicit NotificationStore(QString settingsPath = {});

    static QString defaultSettingsPath();

    // How long a notification stays on disk. Session history is useful across
    // a restart; a month-old launch message is only noise.
    static int retentionDays();

    QString settingsPath() const;

    // Newest first, capped at maxEntries, with anything older than the
    // retention window dropped relative to now.
    QList<NotificationEntry> load(int maxEntries) const;
    bool save(const QList<NotificationEntry> &entries) const;

private:
    QString m_settingsPath;
};
