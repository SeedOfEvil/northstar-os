#include "updateauthorizationcontroller.h"

#include "packagetrustcontroller.h"
#include "updateplancontroller.h"

#include <QFileInfo>
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
    return false;
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
             QStringLiteral("Preflight passed; the privileged update helper is not connected."),
             QStringLiteral("Would create boot environment '%1' before the authorized package transaction; no bectl or pkg command was run.")
                 .arg(bootEnvironmentName),
             bootEnvironmentName);
    return true;
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
