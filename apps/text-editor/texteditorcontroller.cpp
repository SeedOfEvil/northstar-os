#include "texteditorcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr qint64 MaximumDocumentBytes = 8 * 1024 * 1024;

QString displayNameFor(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? QStringLiteral("document") : name;
}

} // namespace

TextEditorController::TextEditorController(QObject *parent)
    : QObject(parent)
    , m_statusMessage(QStringLiteral("Start typing or open a text file from Northstar Files."))
{
}

QString TextEditorController::filePath() const
{
    return m_filePath;
}

QString TextEditorController::text() const
{
    return m_text;
}

bool TextEditorController::dirty() const
{
    return m_text != m_savedText;
}

bool TextEditorController::canSave() const
{
    return dirty();
}

QString TextEditorController::defaultSaveDirectory() const
{
    const QString documentsPath = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    return documentsPath.isEmpty() ? QDir::homePath() : documentsPath;
}

QString TextEditorController::statusMessage() const
{
    return m_statusMessage;
}

bool TextEditorController::loadFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() > MaximumDocumentBytes) {
        m_statusMessage = info.size() > MaximumDocumentBytes
            ? QStringLiteral("This document is larger than the 8 MiB text-editor limit.")
            : QStringLiteral("That text file is no longer available.");
        emit stateChanged();
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusMessage = QStringLiteral("Unable to read %1.").arg(displayNameFor(path));
        emit stateChanged();
        return false;
    }

    m_filePath = info.absoluteFilePath();
    m_text = QString::fromUtf8(file.readAll());
    m_savedText = m_text;
    m_statusMessage = QStringLiteral("Opened %1.").arg(displayNameFor(m_filePath));
    emit stateChanged();
    return true;
}

bool TextEditorController::save()
{
    if (m_filePath.isEmpty()) {
        m_statusMessage = QStringLiteral("Choose a name to save this new document.");
        emit stateChanged();
        return false;
    }

    if (!writeDocument(m_filePath)) {
        m_statusMessage = QStringLiteral("Unable to save %1.").arg(displayNameFor(m_filePath));
        emit stateChanged();
        return false;
    }

    m_savedText = m_text;
    m_statusMessage = QStringLiteral("Saved %1.").arg(displayNameFor(m_filePath));
    emit stateChanged();
    return true;
}

bool TextEditorController::saveAs(const QString &path)
{
    const QFileInfo info(path.trimmed());
    if (path.trimmed().isEmpty() || !info.isAbsolute() || info.fileName().isEmpty()
        || info.fileName() == QStringLiteral(".")
        || info.fileName() == QStringLiteral("..")) {
        m_statusMessage = QStringLiteral("Choose a valid file name.");
        emit stateChanged();
        return false;
    }

    const QString targetPath = info.absoluteFilePath();
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !QDir().mkpath(targetInfo.absolutePath())) {
        m_statusMessage = QStringLiteral("Unable to create the save folder.");
        emit stateChanged();
        return false;
    }

    if (!writeDocument(targetPath)) {
        m_statusMessage = QStringLiteral("Unable to save %1.").arg(displayNameFor(targetPath));
        emit stateChanged();
        return false;
    }

    m_filePath = targetPath;
    m_savedText = m_text;
    m_statusMessage = QStringLiteral("Saved %1.").arg(displayNameFor(m_filePath));
    emit stateChanged();
    return true;
}

bool TextEditorController::writeDocument(const QString &path) const
{
    const QByteArray encodedText = m_text.toUtf8();
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(encodedText) == encodedText.size()
        && file.commit();
}

void TextEditorController::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    emit stateChanged();
}
