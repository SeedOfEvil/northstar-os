#include "packagetrustcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringList>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <utility>

namespace {

constexpr qsizetype MaximumNameLength = 128;
constexpr qsizetype MaximumTagLength = 64;
constexpr qsizetype MaximumUrlLength = 512;
constexpr qsizetype MaximumPathLength = 512;

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isUnresolved(const QString &value)
{
    return value.contains(QStringLiteral("UNSET"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral("RESOLVED_BY_BUILDER"), Qt::CaseInsensitive);
}

} // namespace

PackageTrustController::PackageTrustController(QString policyPath, QObject *parent)
    : QObject(parent)
    , m_policyPath(policyPath.trimmed())
{
    if (m_policyPath.isEmpty()) {
        m_policyPath = defaultPolicyPath();
    }
    reload();
}

bool PackageTrustController::policyPresent() const
{
    return m_policyPresent;
}

bool PackageTrustController::policyValid() const
{
    return m_policyValid;
}

QString PackageTrustController::channel() const
{
    return m_policy.channel;
}

QString PackageTrustController::repositoryTag() const
{
    return m_policy.repositoryTag;
}

QString PackageTrustController::repositoryName() const
{
    return m_policy.repositoryName;
}

QString PackageTrustController::repositoryUrl() const
{
    return m_policy.repositoryUrl;
}

QString PackageTrustController::mirrorType() const
{
    return m_policy.mirrorType;
}

QString PackageTrustController::signatureType() const
{
    return m_policy.signatureType;
}

QString PackageTrustController::fingerprintsPath() const
{
    return m_policy.fingerprintsPath;
}

QString PackageTrustController::policyPath() const
{
    return m_policyPath;
}

QString PackageTrustController::repositoryConfigPreview() const
{
    return m_repositoryConfigPreview;
}

QString PackageTrustController::trustStatus() const
{
    return m_trustStatus;
}

QString PackageTrustController::updatePlanStatus() const
{
    return m_updatePlanStatus;
}

bool PackageTrustController::reload()
{
    m_policy = {};
    m_repositoryConfigPreview.clear();
    m_policyPresent = QFileInfo::exists(m_policyPath);
    m_policyValid = false;

    if (!m_policyPresent) {
        setFailure(QStringLiteral("No signed repository policy is configured."),
                   QStringLiteral("Update planning is blocked until a signed repository policy is configured."));
        return false;
    }

    QFile file(m_policyPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setFailure(QStringLiteral("Repository policy could not be read."),
                   QStringLiteral("Update planning is blocked because the repository policy could not be read."));
        return false;
    }

    QString errorMessage;
    if (!parsePolicy(file.readAll(), m_policy, &errorMessage)) {
        setFailure(QStringLiteral("Repository policy rejected: %1").arg(errorMessage),
                   QStringLiteral("Update planning is blocked until the repository policy is corrected."));
        return false;
    }

    m_policyValid = true;
    m_repositoryConfigPreview = renderPkgRepositoryConfig(m_policy);
    m_trustStatus = QStringLiteral("Policy is structurally valid for pkg fingerprint trust; signature verification is not connected yet.");
    m_updatePlanStatus = QStringLiteral("Update planning is blocked until repository signatures are verified.");
    emit stateChanged();
    return true;
}

void PackageTrustController::planUpdate()
{
    if (!m_policyValid) {
        m_updatePlanStatus = QStringLiteral("Update planning is blocked until a valid signed repository policy is configured.");
    } else {
        m_updatePlanStatus = QStringLiteral("Update planning is blocked until repository signatures are verified.");
    }
    emit stateChanged();
}

bool PackageTrustController::parsePolicy(const QByteArray &contents,
                                         PackageRepositoryPolicy &policy,
                                         QString *errorMessage)
{
    QHash<QString, QString> values;
    const QStringList lines = QString::fromUtf8(contents).split(QLatin1Char('\n'));
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QString line = lines.at(index).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            setError(errorMessage, QStringLiteral("line %1 must use key=value syntax").arg(index + 1));
            return false;
        }

        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (key.isEmpty() || value.isEmpty()) {
            setError(errorMessage, QStringLiteral("line %1 has an empty key or value").arg(index + 1));
            return false;
        }
        if (values.contains(key)) {
            setError(errorMessage, QStringLiteral("key '%1' is duplicated").arg(key));
            return false;
        }
        if (key != QStringLiteral("channel")
            && key != QStringLiteral("repository_tag")
            && key != QStringLiteral("repository_name")
            && key != QStringLiteral("repository_url")
            && key != QStringLiteral("mirror_type")
            && key != QStringLiteral("signature_type")
            && key != QStringLiteral("fingerprints_path")
            && key != QStringLiteral("trust_mode")) {
            setError(errorMessage, QStringLiteral("key '%1' is not part of the policy contract").arg(key));
            return false;
        }
        values.insert(key, value);
    }

    const QStringList requiredKeys{
        QStringLiteral("channel"),
        QStringLiteral("repository_tag"),
        QStringLiteral("repository_name"),
        QStringLiteral("repository_url"),
        QStringLiteral("mirror_type"),
        QStringLiteral("signature_type"),
        QStringLiteral("fingerprints_path"),
        QStringLiteral("trust_mode"),
    };
    for (const QString &key : requiredKeys) {
        if (!values.contains(key)) {
            setError(errorMessage, QStringLiteral("required key '%1' is missing").arg(key));
            return false;
        }
        if (isUnresolved(values.value(key))) {
            setError(errorMessage, QStringLiteral("key '%1' still contains an unresolved value").arg(key));
            return false;
        }
    }

    const QString channel = values.value(QStringLiteral("channel"));
    if (channel != QStringLiteral("development") && channel != QStringLiteral("stable")) {
        setError(errorMessage, QStringLiteral("channel must be development or stable"));
        return false;
    }

    const QString repositoryName = values.value(QStringLiteral("repository_name"));
    if (repositoryName.size() > MaximumNameLength) {
        setError(errorMessage, QStringLiteral("repository_name is too long"));
        return false;
    }

    const QString repositoryTag = values.value(QStringLiteral("repository_tag"));
    static const QRegularExpression tagPattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]*$"));
    if (repositoryTag.size() > MaximumTagLength || !tagPattern.match(repositoryTag).hasMatch()) {
        setError(errorMessage, QStringLiteral("repository_tag must contain only safe UCL tag characters"));
        return false;
    }

    const QString repositoryUrl = values.value(QStringLiteral("repository_url"));
    const QUrl url(repositoryUrl);
    if (repositoryUrl.size() > MaximumUrlLength || !url.isValid()
        || url.scheme() != QStringLiteral("pkg+https") || url.host().isEmpty()
        || repositoryUrl.contains(QRegularExpression(QStringLiteral("\\s")))) {
        setError(errorMessage, QStringLiteral("repository_url must be a pkg+https URL without whitespace"));
        return false;
    }

    const QString mirrorType = values.value(QStringLiteral("mirror_type"));
    if (mirrorType != QStringLiteral("none") && mirrorType != QStringLiteral("srv")) {
        setError(errorMessage, QStringLiteral("mirror_type must be none or srv"));
        return false;
    }

    if (values.value(QStringLiteral("signature_type")) != QStringLiteral("fingerprints")) {
        setError(errorMessage, QStringLiteral("signature_type must be fingerprints"));
        return false;
    }

    const QString fingerprintsPath = values.value(QStringLiteral("fingerprints_path"));
    const QStringList pathParts = fingerprintsPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (fingerprintsPath.size() > MaximumPathLength || !fingerprintsPath.startsWith(QLatin1Char('/'))
        || fingerprintsPath.contains(QRegularExpression(QStringLiteral("\\s")))
        || pathParts.contains(QStringLiteral(".."))) {
        setError(errorMessage, QStringLiteral("fingerprints_path must be an absolute safe path"));
        return false;
    }

    if (values.value(QStringLiteral("trust_mode")) != QStringLiteral("required")) {
        setError(errorMessage, QStringLiteral("trust_mode must be required"));
        return false;
    }

    policy = PackageRepositoryPolicy{
        channel,
        repositoryTag,
        repositoryName,
        repositoryUrl,
        mirrorType,
        values.value(QStringLiteral("signature_type")),
        fingerprintsPath,
        values.value(QStringLiteral("trust_mode")),
    };
    return true;
}

QString PackageTrustController::renderPkgRepositoryConfig(const PackageRepositoryPolicy &policy)
{
    return QStringLiteral("%1: {\n"
                          "    url: \"%2\",\n"
                          "    mirror_type: \"%3\",\n"
                          "    signature_type: \"%4\",\n"
                          "    fingerprints: \"%5\",\n"
                          "    enabled: yes\n"
                          "}\n")
        .arg(policy.repositoryTag,
             policy.repositoryUrl,
             policy.mirrorType,
             policy.signatureType,
             QDir::cleanPath(policy.fingerprintsPath));
}

QString PackageTrustController::defaultPolicyPath()
{
    const QString userConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/repository-policy.conf");
    if (QFileInfo::exists(userConfigPath)) {
        return userConfigPath;
    }

    const QString installedPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/repository-policy.conf"),
        QStandardPaths::LocateFile);
    return installedPath.isEmpty() ? userConfigPath : installedPath;
}

void PackageTrustController::setFailure(const QString &trustStatus, const QString &planStatus)
{
    m_trustStatus = trustStatus;
    m_updatePlanStatus = planStatus;
    emit stateChanged();
}
