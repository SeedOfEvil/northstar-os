#include "shortcutcatalog.h"

#include <QVariantMap>

ShortcutCatalog::ShortcutCatalog(QObject *parent)
    : QObject(parent)
    , m_entries({
          {QStringLiteral("applications"), QStringLiteral("Applications"), QStringLiteral("Meta+K")},
          {QStringLiteral("software"), QStringLiteral("Software Center"), QStringLiteral("Meta+U")},
          {QStringLiteral("files"), QStringLiteral("Files"), QStringLiteral("Meta+E")},
          {QStringLiteral("settings"), QStringLiteral("Settings"), QStringLiteral("Meta+,")},
          {QStringLiteral("terminal"), QStringLiteral("Terminal"), QStringLiteral("Ctrl+Alt+T")},
          {QStringLiteral("browser"), QStringLiteral("Firefox"), QStringLiteral("Meta+B")},
          {QStringLiteral("refresh"), QStringLiteral("Refresh Applications"), QStringLiteral("Meta+R")},
      })
{
}

QVariantList ShortcutCatalog::shortcuts() const
{
    QVariantList result;
    result.reserve(m_entries.size());
    for (const ShortcutEntry &entry : m_entries) {
        result.append(QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("label"), entry.label},
            {QStringLiteral("sequence"), entry.sequence},
        });
    }
    return result;
}

QString ShortcutCatalog::sequenceFor(const QString &commandId) const
{
    for (const ShortcutEntry &entry : m_entries) {
        if (entry.id == commandId) {
            return entry.sequence;
        }
    }
    return {};
}
