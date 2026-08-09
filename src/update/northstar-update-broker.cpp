#include "packagetrustcontroller.h"
#include "updateauthorizationcontroller.h"
#include "updateplancontroller.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QVariantMap>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace {

void printError(const QString &message)
{
    QTextStream(stderr) << "ERROR: " << message << Qt::endl;
}

bool isRoot()
{
#if defined(Q_OS_UNIX)
    return geteuid() == 0;
#else
    return false;
#endif
}

bool isSecureRootPath(const QFileInfo &info, bool directory)
{
    if (!info.exists() || info.isSymLink()) {
        return false;
    }
    if (directory ? !info.isDir() : !info.isFile()) {
        return false;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != 0) {
        return false;
    }
    const QFileDevice::Permissions permissions = info.permissions();
    if (permissions.testFlag(QFileDevice::WriteGroup)
        || permissions.testFlag(QFileDevice::WriteOther)) {
        return false;
    }
#endif
    return true;
}

bool readInstalledSnapshot(const QString &path,
                           QVariantList &installed,
                           QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("installed package snapshot could not be read");
        }
        return false;
    }

    QSet<QString> names;
    const QRegularExpression namePattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9+_.-]*$"));
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char('|'), Qt::KeepEmptyParts);
        if (fields.size() != 2 || fields.at(0).isEmpty() || fields.at(1).isEmpty()
            || !namePattern.match(fields.at(0)).hasMatch()
            || fields.at(1).contains(QRegularExpression(QStringLiteral("\\s")))) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("installed package snapshot must use name|version lines");
            }
            return false;
        }
        if (names.contains(fields.at(0))) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("installed package snapshot repeats '%1'").arg(fields.at(0));
            }
            return false;
        }
        names.insert(fields.at(0));
        installed.append(QVariantMap{
            {QStringLiteral("name"), fields.at(0)},
            {QStringLiteral("version"), fields.at(1)},
            {QStringLiteral("comment"), QStringLiteral("broker snapshot")},
        });
    }
    return true;
}

bool writeRequest(const QString &path,
                  const PackageTrustController &trust,
                  const UpdatePlanController &plan,
                  const QString &bootEnvironment,
                  QString *errorMessage)
{
    QFile request(path);
    if (!request.open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("authorization request could not be created");
        }
        return false;
    }

    const QByteArray contents = QByteArray("protocol=1\n")
        + QByteArray("operation=create-before\n")
        + QByteArray("channel=") + trust.channel().toUtf8() + QByteArray("\n")
        + QByteArray("repository_revision=") + QByteArray::number(plan.repositoryRevision()) + QByteArray("\n")
        + QByteArray("source_revision=") + plan.sourceRevision().toUtf8() + QByteArray("\n")
        + QByteArray("catalogue_sha256=") + plan.catalogueSha256().toUtf8() + QByteArray("\n")
        + QByteArray("signature_fingerprint=") + plan.signatureFingerprint().toUtf8() + QByteArray("\n")
        + QByteArray("boot_environment=") + bootEnvironment.toUtf8() + QByteArray("\n")
        + QByteArray("plan_status=verified\n")
        + QByteArray("authorization=interactive-confirmation\n");
    if (request.write(contents) != contents.size()
        || !request.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("authorization request could not be written securely");
        }
        request.remove();
        return false;
    }
    request.close();
    return true;
}

int capabilities()
{
    QTextStream(stdout)
        << "protocol=1\n"
        << "operation=stage-create-before\n"
        << "verification=policy,fingerprint,catalogue-digest,publication-signature,preview\n"
        << "request=root-owned-mode-0600\n"
        << "mutation=none\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-update-broker"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Northstar verified update-request broker"));
    parser.addHelpOption();

    const QCommandLineOption capabilitiesOption(QStringLiteral("capabilities"),
                                                QStringLiteral("Report the broker contract without changing state."));
    const QCommandLineOption stageOption(QStringLiteral("stage-create-before"),
                                         QStringLiteral("Stage a verified create-before request."));
    const QCommandLineOption confirmOption(QStringLiteral("confirm"),
                                           QStringLiteral("Require explicit interactive confirmation."));
    const QCommandLineOption policyOption({QStringLiteral("p"), QStringLiteral("policy")},
                                          QStringLiteral("Root-owned repository policy path."),
                                          QStringLiteral("path"));
    const QCommandLineOption metadataOption({QStringLiteral("m"), QStringLiteral("metadata")},
                                            QStringLiteral("Root-owned repository metadata path."),
                                            QStringLiteral("path"));
    const QCommandLineOption installedOption({QStringLiteral("i"), QStringLiteral("installed-file")},
                                             QStringLiteral("Root-owned installed package snapshot (name|version per line)."),
                                             QStringLiteral("path"));
    const QCommandLineOption requestOption({QStringLiteral("r"), QStringLiteral("request")},
                                            QStringLiteral("New root-owned authorization request path."),
                                            QStringLiteral("path"));
    const QCommandLineOption bectlOption(QStringLiteral("bectl"),
                                         QStringLiteral("bectl executable path (testing override)."),
                                         QStringLiteral("path"),
                                         QStringLiteral("/sbin/bectl"));
    const QCommandLineOption zfsOption(QStringLiteral("zfs"),
                                       QStringLiteral("zfs executable path (testing override)."),
                                       QStringLiteral("path"),
                                       QStringLiteral("/sbin/zfs"));

    parser.addOption(capabilitiesOption);
    parser.addOption(stageOption);
    parser.addOption(confirmOption);
    parser.addOption(policyOption);
    parser.addOption(metadataOption);
    parser.addOption(installedOption);
    parser.addOption(requestOption);
    parser.addOption(bectlOption);
    parser.addOption(zfsOption);
    parser.process(application);

    if (parser.isSet(capabilitiesOption)) {
        return capabilities();
    }
    if (!parser.isSet(stageOption)) {
        printError(QStringLiteral("select --capabilities or --stage-create-before"));
        return 64;
    }
    if (!isRoot()) {
        printError(QStringLiteral("--stage-create-before requires root"));
        return 77;
    }
    if (!parser.isSet(confirmOption)) {
        printError(QStringLiteral("explicit --confirm is required"));
        return 77;
    }

    const QString policyPath = QFileInfo(parser.value(policyOption)).absoluteFilePath();
    const QString metadataPath = QFileInfo(parser.value(metadataOption)).absoluteFilePath();
    const QString installedPath = QFileInfo(parser.value(installedOption)).absoluteFilePath();
    const QString requestPath = QFileInfo(parser.value(requestOption)).absoluteFilePath();
    if (policyPath.isEmpty() || metadataPath.isEmpty() || installedPath.isEmpty() || requestPath.isEmpty()) {
        printError(QStringLiteral("policy, metadata, installed-file, and request paths are required"));
        return 64;
    }

    const QFileInfo policyInfo(policyPath);
    const QFileInfo metadataInfo(metadataPath);
    const QFileInfo installedInfo(installedPath);
    const QFileInfo metadataDirectory(metadataInfo.absolutePath());
    const QFileInfo requestInfo(requestPath);
    const QFileInfo requestDirectory(requestInfo.absolutePath());
    if (!isSecureRootPath(policyInfo, false)
        || !isSecureRootPath(metadataInfo, false)
        || !isSecureRootPath(installedInfo, false)
        || !isSecureRootPath(metadataDirectory, true)
        || !isSecureRootPath(requestDirectory, true)) {
        printError(QStringLiteral("broker inputs and request directory must be root-owned and not group/other writable"));
        return 65;
    }
    if (requestInfo.exists() || requestInfo.isSymLink()) {
        printError(QStringLiteral("request path must not already exist"));
        return 65;
    }

    PackageTrustController trust(policyPath);
    if (!trust.policyValid() || !trust.trustStoreValid()) {
        printError(QStringLiteral("repository policy and fingerprint store are not valid"));
        return 65;
    }

    UpdatePlanController plan(&trust, metadataPath);
    if (!plan.metadataValid() || !plan.catalogueDigestValid() || !plan.signatureVerified()) {
        printError(QStringLiteral("publication metadata, catalogue, and signature must verify"));
        return 65;
    }

    QVariantList installed;
    QString errorMessage;
    if (!readInstalledSnapshot(installedPath, installed, &errorMessage)) {
        printError(errorMessage);
        return 65;
    }
    if (!plan.preview(installed)) {
        printError(QStringLiteral("verified update preview could not be generated"));
        return 65;
    }
    if (plan.updateCount() == 0 && plan.installCount() == 0) {
        printError(QStringLiteral("no package changes are pending"));
        return 65;
    }

    UpdateAuthorizationController authorization(&trust,
                                                &plan,
                                                parser.value(bectlOption),
                                                parser.value(zfsOption));
    if (!authorization.preflightValid()) {
        printError(QStringLiteral("update safety preflight is not valid"));
        return 65;
    }

    if (!writeRequest(requestPath,
                      trust,
                      plan,
                      authorization.bootEnvironmentName(),
                      &errorMessage)) {
        printError(errorMessage);
        return 65;
    }

    QTextStream(stdout)
        << "STAGED: " << requestPath << Qt::endl
        << "boot_environment=" << authorization.bootEnvironmentName() << Qt::endl
        << "updates=" << plan.updateCount() << Qt::endl
        << "installs=" << plan.installCount() << Qt::endl
        << "No pkg or bectl command was run." << Qt::endl;
    return 0;
}
