#include "applicationbundlepackager.h"
#include "applicationbundleinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr auto privateFile = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
constexpr auto privateDirectory = privateFile | QFileDevice::ExeOwner;

bool safeText(const QJsonValue &value)
{
    if (!value.isString())
        return false;
    const QString text = value.toString();
    if (text.isEmpty() || text.size() > 200 || text != text.trimmed())
        return false;
    for (const QChar c : text) {
        if (c.category() == QChar::Other_Control || c.category() == QChar::Other_Surrogate)
            return false;
    }
    return true;
}

bool exactKeys(const QJsonObject &object, const QStringList &keys)
{
    if (object.size() != keys.size())
        return false;
    for (const QString &key : keys) {
        if (!object.contains(key))
            return false;
    }
    return true;
}

bool relativeInput(const QString &base, const QJsonValue &value, QString *path)
{
    if (!safeText(value))
        return false;
    const QString name = value.toString();
    if (QDir::isAbsolutePath(name) || name.contains(':') || name.contains('\\'))
        return false;
    QString current = base;
    for (const QString &part : name.split('/')) {
        if (part.isEmpty() || part == "." || part == "..")
            return false;
        current = QDir(current).filePath(part);
        if (QFileInfo(current).isSymLink())
            return false;
    }
    *path = current;
    return true;
}

// Validate the open file and avoid blocking on special files. Sources are
// trusted local build directories, not concurrently modified hostile trees.
bool readInput(const QString &path, qint64 limit, QByteArray *data, bool executable = false)
{
#ifdef Q_OS_UNIX
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        return false;
    struct stat info {};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid()
        || (info.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID))
        || info.st_size <= 0 || info.st_size > limit
        || (executable && !(info.st_mode & S_IXUSR))) {
        ::close(fd);
        return false;
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return false;
    }
    *data = file.read(limit + 1);
    return file.error() == QFileDevice::NoError && data->size() == info.st_size;
#else
    Q_UNUSED(path); Q_UNUSED(limit); Q_UNUSED(data); Q_UNUSED(executable);
    return false;
#endif
}

bool writeOutput(const QString &path, const QByteArray &data, bool executable = false)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        && file.setPermissions(executable ? privateDirectory : privateFile)
        && file.write(data) == data.size() && file.flush();
}
} // namespace

bool ApplicationBundlePackager::package(const QString &recipePath, const QString &outputPath,
                                        QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (error)
        error->clear();
    QByteArray recipeBytes;
    if (!readInput(recipePath, 64 * 1024, &recipeBytes))
        return fail("Recipe must be an owned, non-writable-by-others regular file (1-65536 bytes).");
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(recipeBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail("Recipe must be a JSON object.");
    const QJsonObject recipe = document.object();
    if (!exactKeys(recipe, {"schemaVersion", "bundleIdentifier", "displayName", "version",
                            "executable", "icon", "categories", "license", "provenance"})
        || recipe.value("schemaVersion") != QJsonValue(1))
        return fail("Expected recipe schemaVersion 1 and exactly the documented fields.");
    for (const QString &key : {QString("bundleIdentifier"), QString("displayName"), QString("version")}) {
        if (!safeText(recipe.value(key)))
            return fail("Invalid recipe field: " + key);
    }
    if (!QRegularExpression("^[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+)+$")
             .match(recipe.value("bundleIdentifier").toString()).hasMatch())
        return fail("bundleIdentifier must be a reverse-domain identifier.");
    if (!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?$")
             .match(recipe.value("version").toString()).hasMatch())
        return fail("version must use X.Y.Z with an optional prerelease or build suffix.");
    const auto categories = recipe.value("categories").toArray();
    if (categories.isEmpty() || categories.size() > 16)
        return fail("categories must be a nonempty array of at most 16 strings.");
    for (const auto &category : categories) {
        if (!safeText(category))
            return fail("Invalid category.");
    }
    const auto provenance = recipe.value("provenance").toObject();
    if (!exactKeys(provenance, {"source", "package", "revision"}))
        return fail("provenance requires source, package, and revision.");
    for (const auto &value : provenance) {
        if (!safeText(value))
            return fail("Invalid provenance value.");
    }
    const auto license = recipe.value("license").toObject();
    if (!exactKeys(license, {"id", "file"}) || !safeText(license.value("id"))
        || !QRegularExpression("^[A-Za-z0-9][A-Za-z0-9.+-]*$")
             .match(license.value("id").toString()).hasMatch())
        return fail("license requires a single licence identifier and a file.");

    const QString base = QFileInfo(recipePath).absolutePath();
    QString executablePath, iconPath, licensePath;
    if (!relativeInput(base, recipe.value("executable"), &executablePath)
        || !relativeInput(base, recipe.value("icon"), &iconPath)
        || !relativeInput(base, license.value("file"), &licensePath))
        return fail("Inputs must be relative paths below the recipe directory, without symlinks or traversal.");
    QByteArray executable, icon, licenseText;
    if (!readInput(executablePath, 128LL * 1024 * 1024, &executable, true))
        return fail("Executable must be an owned executable regular file, at most 128 MiB.");
    if (!executable.startsWith(QByteArray("\x7f" "ELF", 4)) && !executable.startsWith("#!/"))
        return fail("Executable must be an ELF binary or a script with an absolute shebang; PE/Mach-O are unsupported.");
    const QString iconSuffix = QFileInfo(iconPath).suffix().toLower();
    if (!readInput(iconPath, 8 * 1024 * 1024, &icon)
        || (iconSuffix != "svg" && iconSuffix != "png"))
        return fail("Icon must be an owned SVG or PNG file, at most 8 MiB.");
    if (iconSuffix == "png" && !icon.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))
        return fail("Invalid PNG signature.");
    if (iconSuffix == "svg") {
        QXmlStreamReader reader(icon);
        if (!reader.readNextStartElement() || reader.name() != QStringLiteral("svg"))
            return fail("Icon must contain an SVG root element.");
        while (!reader.atEnd())
            reader.readNext();
        if (reader.hasError())
            return fail("Malformed SVG icon.");
    }
    if (!readInput(licensePath, 1024 * 1024, &licenseText) || licenseText.trimmed().isEmpty()
        || licenseText.contains('\0') || QString::fromUtf8(licenseText).toUtf8() != licenseText)
        return fail("Licence file must be nonempty UTF-8 text, at most 1 MiB.");

    const QFileInfo output(QDir::cleanPath(QFileInfo(outputPath).absoluteFilePath()));
    const QFileInfo parent(output.absolutePath());
    if (!output.fileName().endsWith(".app") || output.fileName() == ".app"
        || output.exists() || output.isSymLink())
        return fail("Output must be a new .app directory; existing output is never replaced.");
    if (!parent.isDir() || parent.isSymLink()
        || parent.ownerId() != QFileInfo(recipePath).ownerId()
        || (parent.permissions() & (QFileDevice::WriteGroup | QFileDevice::WriteOther)))
        return fail("Output parent must already exist and be an owned directory not writable by others.");
    QTemporaryDir staging(QDir(parent.canonicalFilePath()).filePath(".northstar-package-XXXXXX"));
    if (!staging.isValid())
        return fail("Could not create private packaging staging directory.");
    const QString bundle = staging.filePath("bundle.app");
    for (const QString &relative : {QString(), QString("Contents"), QString("Contents/Executable"),
                                    QString("Contents/Resources")}) {
        const QString path = QDir(bundle).filePath(relative);
        if (!QDir().mkpath(path) || !QFile::setPermissions(path, privateDirectory))
            return fail("Could not prepare private bundle directories.");
    }
    QByteArray manifest;
    QXmlStreamWriter writer(&manifest);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("plist");
    writer.writeAttribute("version", "1.0");
    writer.writeStartElement("dict");
    const auto field = [&writer](const QString &key, const QString &value) {
        writer.writeTextElement("key", key);
        writer.writeTextElement("string", value);
    };
    field("BundleIdentifier", recipe.value("bundleIdentifier").toString());
    field("DisplayName", recipe.value("displayName").toString());
    field("Version", recipe.value("version").toString());
    field("Executable", "app");
    field("Icon", "icon." + iconSuffix);
    writer.writeTextElement("key", "Categories");
    writer.writeStartElement("array");
    for (const auto &category : categories)
        writer.writeTextElement("string", category.toString());
    writer.writeEndElement();
    writer.writeTextElement("key", "Provenance");
    writer.writeStartElement("dict");
    field("Source", provenance.value("source").toString());
    field("Package", provenance.value("package").toString());
    field("Revision", provenance.value("revision").toString());
    writer.writeEndElement();
    writer.writeTextElement("key", "License");
    writer.writeStartElement("dict");
    field("Identifier", license.value("id").toString());
    field("File", "LICENSE");
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    if (writer.hasError()
        || !writeOutput(bundle + "/Contents/Info.plist", manifest)
        || !writeOutput(bundle + "/Contents/Executable/app", executable, true)
        || !writeOutput(bundle + "/Contents/Resources/icon." + iconSuffix, icon)
        || !writeOutput(bundle + "/Contents/Resources/LICENSE", licenseText))
        return fail("Could not write the complete staged bundle.");
    ApplicationBundleInstaller validator;
    const auto details = validator.bundleDetails(bundle);
    if (!details.value("valid").toBool())
        return fail("Installer validation failed: " + details.value("validationError").toString());
    if (!QDir().rename(bundle, output.absoluteFilePath()))
        return fail("Could not publish bundle; output may already exist.");
    return true;
}
