#include "applicationbundlecatalog.h"
#include "applicationbundleinstaller.h"
#include "applicationbundlepackager.h"
#include "webapplication.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    QTextStream output(stdout);
    QTextStream error(stderr);

    if (arguments.size() == 4 && arguments.at(1) == QStringLiteral("package")) {
        QString reason;
        if (!ApplicationBundlePackager::package(arguments.at(2), arguments.at(3), &reason)) {
            error << "ERROR: " << reason << '\n';
            return 1;
        }
        output << "Packaged " << arguments.at(3) << ". Not installed or signed.\n";
        return 0;
    }

    if (arguments.size() != 3) {
        error << "usage: northstar-app inspect <bundle.app>\n"
                 "       northstar-app install <bundle.app>\n"
                 "       northstar-app remove <bundle-identifier>\n"
                 "       northstar-app package <recipe.json> <output.app>\n";
        return 64;
    }

    if (arguments.at(1) == QStringLiteral("inspect")) {
        BundleApplication bundle;
        if (!ApplicationBundleCatalog::inspectBundle(arguments.at(2), &bundle)) {
            error << "ERROR: invalid Northstar application bundle\n";
            return 1;
        }
        output << "BundleIdentifier=" << bundle.bundleId << '\n'
               << "DisplayName=" << bundle.name << '\n'
               << "Version=" << bundle.version << '\n'
               << "Source=" << bundle.provenance.source << '\n'
               << "Package=" << bundle.provenance.package << '\n'
               << "Revision=" << bundle.provenance.revision << '\n';
        if (!bundle.webUrl.isEmpty()) {
            output << "Type=Web application\nURL=" << bundle.webUrl
                   << "\nOrigin=" << WebApplication::origin(bundle.webUrl)
                   << '\n' << WebApplication::notice() << '\n';
        }
        return 0;
    }

    ApplicationBundleInstaller installer;
    const bool succeeded = arguments.at(1) == QStringLiteral("install")
        ? installer.installBundle(arguments.at(2))
        : arguments.at(1) == QStringLiteral("remove")
            ? installer.removeBundle(arguments.at(2)) : false;
    if (!succeeded) {
        error << "ERROR: " << (installer.statusMessage().isEmpty()
                                  ? QStringLiteral("unknown command") : installer.statusMessage()) << '\n';
        return installer.statusMessage().isEmpty() ? 64 : 1;
    }
    output << installer.statusMessage() << '\n';
    return 0;
}
