#include "applicationcompatibility.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

class CompatibilityTest : public QObject
{
    Q_OBJECT
private slots:
    void formats_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("status");
        QTest::addColumn<QString>("format");
        QTest::newRow("script") << QByteArray("#!/bin/sh\ntouch SHOULD_NOT_EXIST\n") << QString("unverified") << QString("Interpreter script");
        QTest::newRow("unknown") << QByteArray("hello") << QString("unverified") << QString("Unknown format");
        QTest::newRow("truncated-elf") << QByteArray("\177ELF", 4) << QString("unverified") << QString("ELF");
        QByteArray elf(64, '\0');
        elf.replace(0, 4, QByteArray("\177ELF", 4)); elf[4] = 2; elf[5] = 1; elf[6] = 1; elf[7] = 9; elf[18] = 62;
        QTest::newRow("freebsd") << elf << QString("unverified") << QString("ELF 64-bit");
        elf[7] = 3;
        QTest::newRow("linux") << elf << QString("unverified") << QString("ELF 64-bit");
        elf[4] = 1; elf[5] = 2; elf[18] = 0; elf[19] = 3;
        QTest::newRow("big-endian") << elf << QString("unverified") << QString("ELF 32-bit");
        QByteArray pe(128, '\0'); pe.replace(0, 2, "MZ"); pe[60] = 64; pe.replace(64, 4, QByteArray("PE\0\0", 4));
        QTest::newRow("pe") << pe << QString("unsupported") << QString("PE");
        pe[60] = '\xff'; pe[61] = '\xff'; pe[62] = '\xff'; pe[63] = '\xff';
        QTest::newRow("pe-offset-overflow") << pe << QString("unverified") << QString("Unknown");
        QTest::newRow("mz-only") << QByteArray("MZ") << QString("unverified") << QString("Unknown");
        QByteArray mach(32, '\0'); mach.replace(0, 4, QByteArray::fromHex("cffaedfe"));
        QTest::newRow("mach") << mach << QString("unsupported") << QString("Mach-O");
        mach.replace(0, 4, QByteArray::fromHex("cafebabe"));
        QTest::newRow("java-not-macos") << mach << QString("unverified") << QString("Unknown");
    }
    void formats()
    {
        QFETCH(QByteArray, bytes); QFETCH(QString, status); QFETCH(QString, format);
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString path = dir.filePath("sample.exe");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size()); file.close();
        const auto report = ApplicationCompatibility::report(path);
        QCOMPARE(report.value("status").toString(), status);
        QVERIFY(report.value("format").toString().startsWith(format));
        QCOMPARE(report.value("runtimeVerified").toBool(), false);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), bytes);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }
    void unsafeAndMissing()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QCOMPARE(ApplicationCompatibility::report(dir.filePath("missing.app")).value("status").toString(), "invalid-bundle");
        QVERIFY(QDir().mkdir(dir.filePath("Broken.app")));
        QVERIFY(ApplicationCompatibility::report(dir.filePath("Broken.app")).value("message").toString().contains("Contents"));
#ifdef Q_OS_UNIX
        const QString fifo = dir.filePath("pipe");
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
        QCOMPARE(ApplicationCompatibility::report(fifo).value("format").toString(), "Unreadable");
        QVERIFY(QFile::link(fifo, dir.filePath("link")));
        QCOMPARE(ApplicationCompatibility::report(dir.filePath("link")).value("format").toString(), "Unreadable");
#endif
    }
};
QTEST_GUILESS_MAIN(CompatibilityTest)
#include "test-applicationcompatibility.moc"
