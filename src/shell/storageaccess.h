#pragma once
#include <QMap>
#include <QVariantMap>
#include <QDBusObjectPath>

namespace StorageAccess {
using Interfaces = QMap<QString, QVariantMap>;
using Objects = QMap<QDBusObjectPath, Interfaces>;
struct Snapshot { Objects objects; QString error; };
Snapshot inspect();
QVariantList describe(const Objects &objects);
QString mountPath(uint uid, const QString &device);
}
Q_DECLARE_METATYPE(StorageAccess::Interfaces)
Q_DECLARE_METATYPE(StorageAccess::Objects)
