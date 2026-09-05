#pragma once

#include <QVariantMap>

// Read-only format evidence, never an execution or security guarantee.
namespace ApplicationCompatibility {
QVariantMap report(const QString &path);
}
