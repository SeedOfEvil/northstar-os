#pragma once
#include <QString>
#include <QVariantMap>

namespace WebApplication {
QString normalizedUrl(const QString &input);
QString origin(const QString &url);
bool read(const QVariantMap &fields, QString *url);
QString notice();
QString browserProgram();
}
