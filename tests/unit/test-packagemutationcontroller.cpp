#include "packagemutationcontroller.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QtTest>

class PackageMutationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsStableOpaquePlanIdentifier();
    void rejectsOutOfBoundsPlanComponents();
    void presentsBoundedTransactionResult();
};

void PackageMutationControllerTest::createsStableOpaquePlanIdentifier()
{
    const QByteArray payload = PackageMutationController::recordPayload(
        1787198400,
        QStringLiteral("install"),
        QStringLiteral("FreeBSD-ports"),
        QString(64, QLatin1Char('a')),
        42,
        QStringLiteral("cowsay"),
        QStringLiteral("3.04_3"),
        QStringLiteral("games/cowsay"));
    QCOMPARE(payload,
             QByteArray("protocol=1\n"
                        "timestamp=1787198400\n"
                        "operation=install\n"
                        "repository=FreeBSD-ports\n"
                        "catalogue_sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                        "index=42\n"
                        "name=cowsay\n"
                        "version=3.04_3\n"
                        "origin=games/cowsay\n"));

    const QByteArray recordHash = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    const QByteArray previewHash = QCryptographicHash::hash("exact preview", QCryptographicHash::Sha256);
    const QString identifier = PackageMutationController::planIdentifier(
        1787198400, 42, recordHash, previewHash);
    QCOMPARE(identifier.size(), 149);
    QVERIFY(QRegularExpression(QStringLiteral(
        "^[0-9]{10}-[0-9]{8}-[0-9a-f]{64}-[0-9a-f]{64}$"))
                .match(identifier)
                .hasMatch());
    QVERIFY(!identifier.contains(QStringLiteral("cowsay")));
    QVERIFY(!identifier.contains(QStringLiteral("install")));
}

void PackageMutationControllerTest::rejectsOutOfBoundsPlanComponents()
{
    const QByteArray hash(32, 'x');
    QVERIFY(PackageMutationController::planIdentifier(1, 0, hash, hash).isEmpty());
    QVERIFY(PackageMutationController::planIdentifier(1787198400, -1, hash, hash).isEmpty());
    QVERIFY(PackageMutationController::planIdentifier(1787198400, 0, QByteArray(31, 'x'), hash).isEmpty());
}

void PackageMutationControllerTest::presentsBoundedTransactionResult()
{
    const QByteArray output(
        "pkg progress that must not hide the result\n"
        "PACKAGE_ACTION=remove\n"
        "PACKAGE=cowsay\n"
        "BOOT_ENVIRONMENT=northstar-before-package-1787273774-78dfefe0\n"
        "ROLLBACK_AVAILABLE=yes\n");
    const QString status = PackageMutationController::transactionSuccessStatus(output);

    QVERIFY(status.startsWith(QStringLiteral("Package transaction completed successfully.\n")));
    QVERIFY(!status.contains(QStringLiteral("pkg progress")));
    QVERIFY(status.contains(QStringLiteral("PACKAGE_ACTION=remove")));
    QVERIFY(status.contains(QStringLiteral(
        "BOOT_ENVIRONMENT=northstar-before-package-1787273774-78dfefe0")));
    QCOMPARE(PackageMutationController::transactionSuccessStatus("unstructured output"),
             QStringLiteral("Package transaction completed successfully."));
}

QTEST_MAIN(PackageMutationControllerTest)

#include "test-packagemutationcontroller.moc"
