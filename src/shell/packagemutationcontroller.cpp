#include "packagemutationcontroller.h"

#include "packagecatalog.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileDevice>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <utility>

namespace {

bool boundedPackageField(const QString &value, qsizetype maximum)
{
    if (value.isEmpty() || value.size() > maximum) {
        return false;
    }
    for (const QChar character : value) {
        if (!(character.isLetterOrNumber() || QStringLiteral("+_.-/").contains(character))) {
            return false;
        }
    }
    return true;
}

bool secureRootFile(const QFileInfo &file, bool executable)
{
    const QFileDevice::Permissions permissions = file.permissions();
    return file.isFile() && (!executable || file.isExecutable()) && file.ownerId() == 0
        && !(permissions & (QFileDevice::WriteGroup | QFileDevice::WriteOther));
}

} // namespace

PackageMutationController::PackageMutationController(PackageCatalog *catalog,
                                                       QString packageManagerPath,
                                                       QString transactionPath,
                                                       QString authorizationPath,
                                                       QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_packageManagerPath(std::move(packageManagerPath))
    , m_transactionPath(std::move(transactionPath))
    , m_authorizationPath(std::move(authorizationPath))
    , m_previewProcess(new QProcess(this))
    , m_transactionProcess(new QProcess(this))
{
    if (m_packageManagerPath.isEmpty() && m_catalog != nullptr) {
        m_packageManagerPath = m_catalog->packageManagerPath();
    }
    if (m_transactionPath.isEmpty()) {
        m_transactionPath = QStringLiteral("/usr/local/libexec/northstar-package-transaction");
    }
    if (m_authorizationPath.isEmpty()) {
        m_authorizationPath = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    }
    m_previewProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_transactionProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_previewProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        finishPreview(exitCode, static_cast<int>(exitStatus));
    });
    connect(m_previewProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            setFailure(QStringLiteral("Could not start the package transaction preview."));
        }
    });
    connect(m_transactionProcess, &QProcess::started, this, [this]() {
        m_status = QStringLiteral("Administrator authorization requested.");
        emit stateChanged();
    });
    connect(m_transactionProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_busy) {
            return;
        }
        const QString output = QString::fromUtf8(m_transactionProcess->readAll()).trimmed();
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        m_busy = false;
        m_planReady = false;
        m_planIdentifier.clear();
        if (success) {
            m_status = output.isEmpty()
                ? QStringLiteral("Package transaction completed successfully.")
                : output.left(400);
        } else if (exitStatus == QProcess::NormalExit && exitCode == 126) {
            m_status = QStringLiteral("Administrator authorization was cancelled.");
        } else {
            m_status = output.isEmpty()
                ? QStringLiteral("Package transaction failed with exit code %1.").arg(exitCode)
                : QStringLiteral("Package transaction failed: %1").arg(output.left(360));
        }
        emit stateChanged();
        emit transactionFinished(success);
    });
    connect(m_transactionProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            setFailure(QStringLiteral("Could not start the protected package service."));
            emit transactionFinished(false);
        }
    });
    refresh();
}

bool PackageMutationController::authorizationAvailable() const { return m_authorizationAvailable; }
bool PackageMutationController::busy() const { return m_busy; }
bool PackageMutationController::planReady() const { return m_planReady; }
QString PackageMutationController::status() const { return m_status; }
QString PackageMutationController::preview() const { return m_preview; }
QString PackageMutationController::operation() const { return m_operation; }
QString PackageMutationController::packageName() const { return m_selectedPackage.value(QStringLiteral("name")).toString(); }

bool PackageMutationController::refresh()
{
    const QFileInfo transaction(m_transactionPath);
    const QFileInfo authorization(m_authorizationPath);
    const QFileInfo policy(QStringLiteral(
        "/usr/local/share/polkit-1/actions/org.northstar.package.policy"));
    m_authorizationAvailable = secureRootFile(transaction, true)
        && secureRootFile(policy, false)
        && authorization.isFile() && authorization.isExecutable();
    if (!m_authorizationAvailable && !m_busy) {
        m_status = QStringLiteral("Install the protected package service to authorize changes.");
    }
    emit stateChanged();
    return m_authorizationAvailable;
}

bool PackageMutationController::planInstall(const QVariantMap &package)
{
    return beginPlan(QStringLiteral("install"), package);
}

bool PackageMutationController::planRemove(const QVariantMap &package)
{
    return beginPlan(QStringLiteral("remove"), package);
}

bool PackageMutationController::beginPlan(const QString &operation, const QVariantMap &package)
{
    if (m_busy || m_catalog == nullptr || !m_catalog->availableCatalogReady()
        || m_packageManagerPath.isEmpty()) {
        return false;
    }
    const QString name = package.value(QStringLiteral("name")).toString();
    const QString version = package.value(QStringLiteral("version")).toString();
    const QString origin = package.value(QStringLiteral("origin")).toString();
    const QString repository = package.value(QStringLiteral("repository")).toString();
    const bool installed = package.value(QStringLiteral("installed"), true).toBool();
    const bool automatic = package.value(QStringLiteral("automatic")).toBool();
    const bool locked = package.value(QStringLiteral("locked")).toBool();
    const int index = package.value(QStringLiteral("planIndex"), -1).toInt();
    if (!boundedPackageField(name, 128) || !boundedPackageField(version, 128)
        || !boundedPackageField(origin, 192) || repository != m_catalog->repositoryName()
        || index < 0 || name == QLatin1String("pkg")
        || (operation == QLatin1String("install") && installed)
        || (operation == QLatin1String("remove") && (!installed || automatic || locked))) {
        return false;
    }
    clearPlan();
    m_busy = true;
    m_operation = operation;
    m_selectedPackage = package;
    m_status = operation == QLatin1String("install")
        ? QStringLiteral("Preparing exact install preview...")
        : QStringLiteral("Preparing exact removal preview...");
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_previewProcess->setProcessEnvironment(environment);
    m_previewProcess->setProgram(m_packageManagerPath);
    // -n keeps the request non-mutating; -y lets pkg finish printing that
    // dry-run as a successful preview instead of reporting user cancellation.
    if (operation == QLatin1String("install")) {
        m_previewProcess->setArguments({QStringLiteral("-o"),
                                        QStringLiteral("REPO_AUTOUPDATE=false"),
                                        QStringLiteral("install"), QStringLiteral("-n"),
                                        QStringLiteral("-y"), QStringLiteral("-r"), repository, name});
    } else {
        m_previewProcess->setArguments({QStringLiteral("-o"),
                                        QStringLiteral("REPO_AUTOUPDATE=false"),
                                        QStringLiteral("delete"), QStringLiteral("-n"),
                                        QStringLiteral("-y"), name});
    }
    m_previewProcess->start(QIODevice::ReadOnly);
    emit stateChanged();
    return true;
}

void PackageMutationController::finishPreview(int exitCode, int exitStatus)
{
    if (!m_busy) {
        return;
    }
    const QByteArray output = m_previewProcess->readAll().trimmed();
    if (exitStatus != static_cast<int>(QProcess::NormalExit) || exitCode != 0 || output.isEmpty()) {
        setFailure(QStringLiteral("FreeBSD pkg could not prepare the requested transaction preview."));
        return;
    }
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();
    const int index = m_selectedPackage.value(QStringLiteral("planIndex")).toInt();
    const QByteArray payload = recordPayload(
        timestamp,
        m_operation,
        m_catalog->repositoryName(),
        m_catalog->catalogueDigest(),
        index,
        m_selectedPackage.value(QStringLiteral("name")).toString(),
        m_selectedPackage.value(QStringLiteral("version")).toString(),
        m_selectedPackage.value(QStringLiteral("origin")).toString());
    m_planIdentifier = planIdentifier(timestamp,
                                      index,
                                      QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
                                      QCryptographicHash::hash(output, QCryptographicHash::Sha256));
    m_preview = QString::fromUtf8(output).left(4000);
    m_busy = false;
    m_planReady = !m_planIdentifier.isEmpty();
    m_status = m_authorizationAvailable
        ? QStringLiteral("Review the exact transaction before administrator authorization.")
        : QStringLiteral("Preview ready, but the protected package service is unavailable.");
    emit stateChanged();
}

bool PackageMutationController::applyPlan()
{
    if (m_busy || !m_planReady || !m_authorizationAvailable || m_planIdentifier.isEmpty()) {
        return false;
    }
    m_busy = true;
    m_status = QStringLiteral("Preparing administrator authorization...");
    m_transactionProcess->setProgram(m_authorizationPath);
    m_transactionProcess->setArguments({m_transactionPath,
                                        QStringLiteral("--apply-plan"),
                                        m_planIdentifier,
                                        QStringLiteral("--confirm")});
    m_transactionProcess->start(QIODevice::ReadOnly);
    emit stateChanged();
    return true;
}

void PackageMutationController::clearPlan()
{
    if (m_busy) {
        return;
    }
    m_selectedPackage.clear();
    m_preview.clear();
    m_operation.clear();
    m_planIdentifier.clear();
    m_planReady = false;
    emit stateChanged();
}

QByteArray PackageMutationController::recordPayload(qint64 timestamp,
                                                     const QString &operation,
                                                     const QString &repository,
                                                     const QString &catalogueDigest,
                                                     int index,
                                                     const QString &name,
                                                     const QString &version,
                                                     const QString &origin)
{
    return QStringLiteral("protocol=1\ntimestamp=%1\noperation=%2\nrepository=%3\n"
                          "catalogue_sha256=%4\nindex=%5\nname=%6\nversion=%7\norigin=%8\n")
        .arg(timestamp)
        .arg(operation, repository, catalogueDigest)
        .arg(index)
        .arg(name, version, origin)
        .toUtf8();
}

QString PackageMutationController::planIdentifier(qint64 timestamp,
                                                   int index,
                                                   const QByteArray &recordHash,
                                                   const QByteArray &previewHash)
{
    if (timestamp < 1000000000 || timestamp > 9999999999LL || index < 0 || index > 99999999
        || recordHash.size() != 32 || previewHash.size() != 32) {
        return {};
    }
    return QStringLiteral("%1-%2-%3-%4")
        .arg(timestamp, 10, 10, QLatin1Char('0'))
        .arg(index, 8, 10, QLatin1Char('0'))
        .arg(QString::fromLatin1(recordHash.toHex()), QString::fromLatin1(previewHash.toHex()));
}

void PackageMutationController::setFailure(const QString &message)
{
    m_busy = false;
    m_planReady = false;
    m_planIdentifier.clear();
    m_status = message;
    emit stateChanged();
}
