#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

struct ShortcutEntry
{
    QString id;
    QString label;
    QString sequence;
};

class ShortcutCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList shortcuts READ shortcuts CONSTANT)

public:
    explicit ShortcutCatalog(QObject *parent = nullptr);

    QVariantList shortcuts() const;
    Q_INVOKABLE QString sequenceFor(const QString &commandId) const;

private:
    QList<ShortcutEntry> m_entries;
};
