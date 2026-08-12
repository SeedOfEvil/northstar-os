#include "firstbootcontroller.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

const QStringList SupportedLocales{
    QStringLiteral("en_US.UTF-8"),
    QStringLiteral("en_CA.UTF-8"),
    QStringLiteral("en_GB.UTF-8"),
    QStringLiteral("es_ES.UTF-8"),
    QStringLiteral("fr_FR.UTF-8"),
    QStringLiteral("de_DE.UTF-8"),
};

const QStringList SupportedTimezones{
    QStringLiteral("UTC"),
    QStringLiteral("America/Denver"),
    QStringLiteral("America/Los_Angeles"),
    QStringLiteral("America/Chicago"),
    QStringLiteral("America/New_York"),
    QStringLiteral("America/Toronto"),
    QStringLiteral("Europe/London"),
    QStringLiteral("Europe/Paris"),
    QStringLiteral("Europe/Berlin"),
};

const QStringList SupportedKeyboards{
    QStringLiteral("us"),
    QStringLiteral("uk"),
    QStringLiteral("es"),
    QStringLiteral("fr"),
    QStringLiteral("de"),
};

void clearBytes(QByteArray &bytes)
{
    bytes.fill('\0');
    bytes.clear();
}

} // namespace

FirstBootController::FirstBootController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_statusMessage(QStringLiteral("Create the first administrator for this Northstar installation."))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::started, this, [this]() {
        m_process->write(m_pendingPassword);
        m_process->write("\n", 1);
        m_process->closeWriteChannel();
        clearBytes(m_pendingPassword);
        emit secretsCleared();
        m_statusMessage = QStringLiteral("Administrator provisioning is in progress.");
        emit stateChanged();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_busy) {
            clearBytes(m_pendingPassword);
            emit secretsCleared();
            finishProvisioning(false, QStringLiteral("Could not start the protected setup service."));
        }
    });
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_busy) {
            return;
        }
        clearBytes(m_pendingPassword);
        emit secretsCleared();
        const QString output = QString::fromUtf8(m_process->readAll()).trimmed();
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        if (success) {
            finishProvisioning(true, QStringLiteral("Setup is complete. Restart to sign in with your new account."));
        } else if (exitStatus == QProcess::NormalExit && exitCode == 126) {
            finishProvisioning(false, QStringLiteral("Protected setup was cancelled."));
        } else {
            finishProvisioning(false,
                               output.isEmpty()
                                   ? QStringLiteral("Protected setup failed with exit code %1.").arg(exitCode)
                                   : QStringLiteral("Protected setup failed: %1").arg(output.left(240)));
        }
    });
}

bool FirstBootController::busy() const { return m_busy; }
QString FirstBootController::statusMessage() const { return m_statusMessage; }
QStringList FirstBootController::locales() const { return SupportedLocales; }
QStringList FirstBootController::timezones() const { return SupportedTimezones; }
QStringList FirstBootController::keyboardLayouts() const { return SupportedKeyboards; }

QString FirstBootController::validateProfile(const QString &displayName,
                                               const QString &username,
                                               const QString &password,
                                               const QString &passwordConfirmation,
                                               const QString &locale,
                                               const QString &timezone,
                                               const QString &keyboardLayout) const
{
    const QString cleanName = displayName.trimmed();
    const QString cleanUser = username.trimmed();
    static const QRegularExpression UsernamePattern(QStringLiteral("^[a-z][a-z0-9_-]{0,30}$"));
    if (cleanName.isEmpty() || cleanName.size() > 80 || containsControlCharacters(cleanName)) {
        return QStringLiteral("Enter a display name between 1 and 80 printable characters.");
    }
    if (!UsernamePattern.match(cleanUser).hasMatch()) {
        return QStringLiteral("Username must start with a lowercase letter and use only lowercase letters, numbers, _ or -.");
    }
    if (password.size() < 8 || password.size() > 128 || containsControlCharacters(password)) {
        return QStringLiteral("Password must contain between 8 and 128 characters without control characters.");
    }
    if (password != passwordConfirmation) {
        return QStringLiteral("Passwords do not match.");
    }
    if (!SupportedLocales.contains(locale)) {
        return QStringLiteral("Choose a supported locale.");
    }
    if (!SupportedTimezones.contains(timezone)) {
        return QStringLiteral("Choose a supported timezone.");
    }
    if (!SupportedKeyboards.contains(keyboardLayout)) {
        return QStringLiteral("Choose a supported keyboard layout.");
    }
    return {};
}

bool FirstBootController::provision(const QString &displayName,
                                    const QString &username,
                                    const QString &password,
                                    const QString &passwordConfirmation,
                                    const QString &locale,
                                    const QString &timezone,
                                    const QString &keyboardLayout)
{
    if (m_busy) {
        return false;
    }
    const QString validation = validateProfile(displayName, username, password,
                                               passwordConfirmation, locale,
                                               timezone, keyboardLayout);
    if (!validation.isEmpty()) {
        m_statusMessage = validation;
        emit stateChanged();
        return false;
    }

    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString requestRoot = runtime.isEmpty() ? QDir::tempPath() : runtime;
    m_request = new QTemporaryFile(requestRoot + QStringLiteral("/northstar-first-boot-XXXXXX.conf"), this);
    m_request->setAutoRemove(true);
    if (!m_request->open() || !m_request->setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        m_request->deleteLater();
        m_request = nullptr;
        m_statusMessage = QStringLiteral("Could not create the protected setup request.");
        emit stateChanged();
        return false;
    }
    const QByteArray request = QStringLiteral(
        "protocol=1\nusername=%1\ndisplay_name=%2\nlocale=%3\ntimezone=%4\nkeyboard=%5\nadmin_confirmation=yes\n")
        .arg(username.trimmed(), displayName.trimmed(), locale, timezone, keyboardLayout)
        .toUtf8();
    if (m_request->write(request) != request.size() || !m_request->flush()) {
        m_request->deleteLater();
        m_request = nullptr;
        m_statusMessage = QStringLiteral("Could not write the protected setup request.");
        emit stateChanged();
        return false;
    }

    QString authorization = qEnvironmentVariable("NORTHSTAR_FIRST_BOOT_AUTH_COMMAND");
    QStringList arguments;
    if (authorization.isEmpty()) {
        authorization = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        arguments << QStringLiteral("/usr/local/libexec/northstar-first-boot-provision");
    }
    if (authorization.isEmpty()) {
        m_request->deleteLater();
        m_request = nullptr;
        m_statusMessage = QStringLiteral("The protected setup service is not installed.");
        emit stateChanged();
        return false;
    }
    arguments << QStringLiteral("--apply") << m_request->fileName();
    m_pendingPassword = password.toUtf8();
    m_busy = true;
    m_statusMessage = QStringLiteral("Starting protected one-time setup...");
    emit stateChanged();
    m_process->setProgram(authorization);
    m_process->setArguments(arguments);
    m_process->start();
    return true;
}

bool FirstBootController::containsControlCharacters(const QString &value)
{
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            || character == QChar::LineSeparator
            || character == QChar::ParagraphSeparator) {
            return true;
        }
    }
    return false;
}

void FirstBootController::finishProvisioning(bool success, const QString &message)
{
    m_busy = false;
    m_statusMessage = message;
    if (m_request != nullptr) {
        m_request->deleteLater();
        m_request = nullptr;
    }
    emit stateChanged();
    emit provisioningFinished(success);
}
