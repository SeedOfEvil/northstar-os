#include "packagecatalog.h"

#include <QtTest>

class PackageCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesAndSortsPackageQueryOutput();
    void ignoresMalformedAndDuplicateRows();
    void filtersAcrossPackageFields();
};

void PackageCatalogTest::parsesAndSortsPackageQueryOutput()
{
    const QList<InstalledPackage> packages = PackageCatalog::parseQueryOutput(
        QByteArray("zlib|1.3|Compression library\n"
                   "bash|5.2|Bourne shell\n"));

    QCOMPARE(packages.size(), 2);
    QCOMPARE(packages.at(0).name, QStringLiteral("bash"));
    QCOMPARE(packages.at(0).version, QStringLiteral("5.2"));
    QCOMPARE(packages.at(1).name, QStringLiteral("zlib"));
}

void PackageCatalogTest::ignoresMalformedAndDuplicateRows()
{
    const QList<InstalledPackage> packages = PackageCatalog::parseQueryOutput(
        QByteArray("invalid\n"
                   "|1.0|missing name\n"
                   "qt6||missing version\n"
                   "qt6|6.8|first|with a pipe\n"
                   "qt6|6.8|duplicate\n"));

    QCOMPARE(packages.size(), 1);
    QCOMPARE(packages.constFirst().comment, QStringLiteral("first|with a pipe"));
}

void PackageCatalogTest::filtersAcrossPackageFields()
{
    const QList<InstalledPackage> packages{
        {QStringLiteral("qt6-qtbase"), QStringLiteral("6.8.2"), QStringLiteral("Qt application framework")},
        {QStringLiteral("qterminal"), QStringLiteral("1.4"), QStringLiteral("Terminal emulator")},
    };

    QCOMPARE(PackageCatalog::filterPackages(packages, QStringLiteral("qt framework")).size(), 1);
    QCOMPARE(PackageCatalog::filterPackages(packages, QStringLiteral("TERMINAL")).constFirst().name,
             QStringLiteral("qterminal"));
    QCOMPARE(PackageCatalog::filterPackages(packages, QString()).size(), 2);
}

QTEST_MAIN(PackageCatalogTest)

#include "test-packagecatalog.moc"
