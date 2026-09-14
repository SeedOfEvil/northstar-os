#pragma once
#include <QSet>
#include <QVariantList>

namespace RemovableStorage {
struct Scan { QVariantList devices; QString status; QVariantList partitions; };
QVariantList parse(const QByteArray &xml, const QSet<QString> &removable, bool *valid);
Scan scan();
}
