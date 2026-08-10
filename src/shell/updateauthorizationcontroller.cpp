#include "updateauthorizationcontroller.h"

#include "packagetrustcontroller.h"
#include "updateplancontroller.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

UpdateAuthorizationController::UpdateAuthorizationController(PackageTrustController *trustController,
                                                               UpdatePlanController *updatePlan,
                                                               QString bectlPath,
                                                               QString zfsPath,
                                                               QObject *parent)
    : QObject(parent)
    , m_trustController(trustController)
    , m_updatePlan(updatePlan)
    , m_bectlPath(std::move(bectlPath))
    , m_zfsPath(std::move(zfsPath))
{
    if (m_trustController != nullptr) {
        connect(m_trustController, &PackageTrustController::stateChanged,
                this, &UpdateAuthorizationController::refresh);
    }
    if (m_updatePlan != nullptr) {
        connect(m_updatePlan, &UpdatePlanController::stateChanged,
                this, &UpdateAuthorizationController::refresh);
    }
    refresh();
}

bool UpdateAuthorizationController::preflightValid() const
{
    return m_preflightValid;
}

bool UpdateAuthorizationController::bectlAvailable() const
{
    return m_bectlAvailable;
}

bool UpdateAuthorizationController::zfsAvailable() const
{
    return m_zfsAvailable;
}

bool UpdateAuthorizationController::authorizationAvailable() const
{
    return m_authorizationAvailable;
}

QString UpdateAuthorizationController::bootEnvironmentName() const
{
    return m_bootEnvironmentName;
}

QString UpdateAuthorizationController::status() const
{
    return m_status;
}

QString UpdateAuthorizationController::plan() const
{
    return m_plan;
}

bool UpdateAuthorizationController::refresh()
{
    const QFileInfo transaction(QStringLiteral("/usr/local/libexec/northstar-update-transaction"));
    const QFileInfo broker(QStringLiteral("/usr/local/libexec/northstar-update-broker"));
    const bool privilegeAvailable = !QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty();
    m_authorizationAvailable = transaction.isFile() && transaction.isExecutable()
        && broker.isFile() && broker.isExecutable() && privilegeAvailable;
    const bool bectlAvailable = executableAvailable(m_bectlPath, QStringLiteral("bectl"));
    const bool zfsAvailable = executableAvailable(m_zfsPath, QStringLiteral("zfs"));
    if (m_updatePlan == nullptr) {
        setState(false,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Update plan is unavailable."),
                 QStringLiteral("No update transaction can be prepared."));
        return false;
    }

    if (m_trustController == nullptr
        || !m_trustController->policyValid()
        || !m_trustController->trustStoreValid()) {
        setState(false,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Blocked: a valid repository policy and fingerprint store are required."),
                 QStringLiteral("No update transaction can be prepared."));
        return false;
    }

    if (!m_updatePlan->metadataValid()
        || !m_updatePlan->catalogueDigestValid()
        || !m_updatePlan->signatureVerified()) {
        setState(false,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Blocked: verified publication metadata and catalogue integrity are required."),
                 QStringLiteral("No update transaction can be prepared."));
        return false;
    }

    if (!m_updatePlan->previewReady()) {
        setState(false,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Blocked: generate an update preview before authorization."),
                 QStringLiteral("No update transaction can be prepared."));
        return false;
    }

    if (!bectlAvailable || !zfsAvailable) {
        setState(false,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Blocked: bectl and zfs must be available before an update can be authorized."),
                 QStringLiteral("No boot environment would be created until both tools are available."));
        return false;
    }

    if (m_updatePlan->updateCount() == 0 && m_updatePlan->installCount() == 0) {
        setState(true,
                 bectlAvailable,
                 zfsAvailable,
                 QStringLiteral("Preflight passed; no package changes are pending."),
                 QStringLiteral("No boot environment is required."));
        return true;
    }

    const QString bootEnvironmentName = makeBootEnvironmentName(*m_updatePlan,
                                                                 m_trustController->channel());
    setState(true,
             bectlAvailable,
             zfsAvailable,
             m_authorizationAvailable
                 ? QStringLiteral("Authorized transaction path is ready.")
                 : QStringLiteral("Preflight passed; install the protected transaction service to continue."),
             QStringLiteral("Create boot environment '%1', apply only verified repository packages, and retain rollback until reboot.")
                 .arg(bootEnvironmentName),
             bootEnvironmentName);
    return true;
}

bool UpdateAuthorizationController::applyUpdate()
{
    if (!m_preflightValid || !m_authorizationAvailable || m_updatePlan == nullptr
        || (m_updatePlan->updateCount() == 0 && m_updatePlan->installCount() == 0)) {
        return false;
    }
    return requestTransaction(QStringLiteral("--apply-update"));
}

bool UpdateAuthorizationController::scheduleRollback()
{
    if (!m_authorizationAvailable) {
        return false;
    }
    return requestTransaction(QStringLiteral("--rollback"));
}

bool UpdateAuthorizationController::requestTransaction(const QString &operation)
{
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        return false;
    }
    const bool started = QProcess::startDetached(
        pkexec,
        {QStringLiteral("/usr/local/libexec/northstar-update-transaction"),
         operation,
         QStringLiteral("--confirm")});
    if (started) {
        m_status = operation == QStringLiteral("--rollback")
            ? QStringLiteral("Rollback authorization requested; reboot after it completes.")
            : QStringLiteral("Update authorization requested.");
        emit stateChanged();
    }
    return started;
}

bool UpdateAuthorizationController::executableAvailable(const QString &overridePath, const QString &name)
{
    if (!overridePath.trimmed().isEmpty()) {
        const QFileInfo fileInfo(overridePath.trimmed());
        return fileInfo.isFile() && fileInfo.isExecutable();
    }
    return !QStandardPaths::findExecutable(name).isEmpty();
}

QString UpdateAuthorizationController::makeBootEnvironmentName(const UpdatePlanController &updatePlan,
                                                                const QString &channel)
{
    return QStringLiteral("northstar-before-%1-r%2-%3")
        .arg(channel,
             QString::number(updatePlan.repositoryRevision()),
             updatePlan.sourceRevision().left(12).toLower());
}

void UpdateAuthorizationController::setState(bool preflightValid,
                                              bool bectlAvailable,
                                              bool zfsAvailable,
                                              const QString &status,
                                              const QString &plan,
                                              const QString &bootEnvironmentName)
{
    m_preflightValid = preflightValid;
    m_bectlAvailable = bectlAvailable;
    m_zfsAvailable = zfsAvailable;
    m_status = status;
    m_plan = plan;
    m_bootEnvironmentName = bootEnvironmentName;
    emit stateChanged();
}
