#pragma once
#include <QString>

// Developer tooling: consumes finished build inputs, never executes them.
class ApplicationBundlePackager
{
public:
    static bool package(const QString &recipePath, const QString &outputPath, QString *error);
};
