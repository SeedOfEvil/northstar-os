#include "applicationlauncher.h"

#include <QProcess>

#include <utility>

ApplicationLauncher::ApplicationLauncher(
    QObject *parent,
    LaunchFunction launchFunction,
    QStringList applicationDirectories)
    : QObject(parent)
    , m_catalog(std::move(applicationDirectories))
    , m_launchFunction(std::move(launchFunction))
{
    if (!m_launchFunction) {
        m_launchFunction = [](const QString &program, const QStringList &arguments) {
            return QProcess::startDetached(program, arguments);
        };
    }

    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::applicationsChanged);
    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::matchingApplicationsChanged);
}

QVariantList ApplicationLauncher::applications() const
{
    return m_catalog.applications();
}

QString ApplicationLauncher::applicationQuery() const
{
    return m_applicationQuery;
}

QVariantList ApplicationLauncher::matchingApplications() const
{
    return m_catalog.searchApplications(m_applicationQuery);
}

bool ApplicationLauncher::launchTerminal() const
{
    return launch(QStringLiteral("qterminal"), {});
}

bool ApplicationLauncher::launchBrowser() const
{
    return launch(QStringLiteral("firefox"), {});
}

bool ApplicationLauncher::launchApplication(const QString &desktopId) const
{
    QString program;
    QStringList arguments;
    if (!m_catalog.launchSpec(desktopId, &program, &arguments)) {
        return false;
    }

    return launch(program, arguments);
}

bool ApplicationLauncher::refreshApplications()
{
    return m_catalog.reload();
}

void ApplicationLauncher::setApplicationQuery(const QString &query)
{
    if (m_applicationQuery == query) {
        return;
    }

    m_applicationQuery = query;
    emit applicationQueryChanged();
    emit matchingApplicationsChanged();
}

bool ApplicationLauncher::launch(const QString &program, const QStringList &arguments) const
{
    return m_launchFunction(program, arguments);
}
