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
    , m_transactionProcess(new QProcess(this))
{
    m_transactionProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_transactionProcess, &QProcess::started, this, [this]() {
        m_transactionStatus = QStringLiteral("Administrator authorization requested.");
        emit transactionStateChanged();
    });
    connect(m_transactionProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_transactionBusy) {
            return;
        }
        m_transactionBusy = false;
        m_transactionStatus = QStringLiteral("Could not start the protected update service.");
        emit transactionStateChanged();
        emit transactionFinished(false);
    });
    connect(m_transactionProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_transactionBusy) {
            return;
        }
        const QString output = QString::fromUtf8(m_transactionProcess->readAll()).trimmed();
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        m_transactionBusy = false;
        if (success) {
            m_transactionStatus = output.isEmpty()
                ? QStringLiteral("Protected transaction completed successfully.")
                : output.left(320);
        } else if (exitStatus == QProcess::NormalExit && exitCode == 126) {
            m_transactionStatus = QStringLiteral("Administrator authorization was cancelled.");
        } else {
            m_transactionStatus = output.isEmpty()
                ? QStringLiteral("Protected transaction failed with exit code %1.").arg(exitCode)
                : QStringLiteral("Protected transaction failed: %1").arg(output.left(280));
        }
        emit transactionStateChanged();
        emit transactionFinished(success);
    });
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

bool UpdateAuthorizationController::transactionBusy() const
{
    return m_transactionBusy;
}

QString UpdateAuthorizationController::transactionStatus() const
{
    return m_transactionStatus;
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
    if (m_transactionBusy) {
        return false;
    }
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        return false;
    }
    m_transactionBusy = true;
    m_transactionStatus = operation == QStringLiteral("--rollback")
        ? QStringLiteral("Preparing rollback authorization...")
        : QStringLiteral("Preparing update authorization...");
    emit transactionStateChanged();
    m_transactionProcess->setProgram(pkexec);
    m_transactionProcess->setArguments(
        {QStringLiteral("/usr/local/libexec/northstar-update-transaction"),
         operation,
         QStringLiteral("--confirm")});
    m_transactionProcess->start();
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
