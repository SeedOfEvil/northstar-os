#include "webapplication.h"
#include <QMetaType>
#include <QUrl>

QString WebApplication::normalizedUrl(const QString &input)
{
    if (input.isEmpty() || input.size() > 200 || input != input.trimmed())
        return {};
    for (const QChar c : input) {
        if (c.isSpace() || c.category() == QChar::Other_Control
            || c.category() == QChar::Other_Format || c.category() == QChar::Other_Surrogate
            || c == QLatin1Char('\\'))
            return {};
    }
    const QUrl url(input, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()
        || !url.userInfo().isEmpty() || url.authority().contains(QLatin1Char('@')))
        return {};
    const QString encoded = url.toString(QUrl::FullyEncoded);
    return encoded.size() <= 200 ? encoded : QString();
}

QString WebApplication::origin(const QString &url)
{
    return QUrl(url).adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
        .toString(QUrl::FullyEncoded);
}

bool WebApplication::read(const QVariantMap &fields, QString *url)
{
    if (!url || fields.size() != 5)
        return false;
    const QVariantMap required = {{"Browser", "firefox"}, {"Network", "required"},
                                 {"Storage", "shared-browser-profile"}, {"Permissions", "browser-managed"}};
    for (auto it = required.cbegin(); it != required.cend(); ++it) {
        const QVariant value = fields.value(it.key());
        if (value.typeId() != QMetaType::QString || value != it.value())
            return false;
    }
    if (fields.value("URL").typeId() != QMetaType::QString)
        return false;
    *url = normalizedUrl(fields.value("URL").toString());
    return !url->isEmpty();
}

QString WebApplication::notice()
{
    return QStringLiteral("Web app • Opens in Firefox • Internet required. Shares Firefox cookies, "
                          "storage and browser-managed permissions. Links may leave this website. "
                          "Removing this app does not clear browser data.");
}

QString WebApplication::browserProgram()
{
    // Use the platform package location, not an executable supplied by a bundle
    // or a writable PATH entry. Missing Firefox is a normal launch failure.
    return QStringLiteral("/usr/local/bin/firefox");
}
