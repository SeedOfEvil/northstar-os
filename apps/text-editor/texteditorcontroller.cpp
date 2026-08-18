#include "texteditorcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>

namespace {

constexpr qint64 MaximumDocumentBytes = 8 * 1024 * 1024;
constexpr int MaximumBrowseEntries = 2000;

QString sizeSummary(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KiB").arg(bytes / 1024);
    }
    return QStringLiteral("%1 MiB").arg(bytes / (1024 * 1024));
}

} // namespace

TextEditorController::TextEditorController(QObject *parent, QString recentFilesPath)
    : QObject(parent)
    , m_recentFiles(std::move(recentFilesPath))
{
    refreshRecentFiles();
    newDocument();
    browseHome();
    announce(QStringLiteral("Start typing, or open a text file from Northstar Files."));
}

qint64 TextEditorController::maximumDocumentBytes()
{
    return MaximumDocumentBytes;
}

QString TextEditorController::normalizedPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QFileInfo(QDir::cleanPath(QDir::fromNativeSeparators(trimmed))).absoluteFilePath();
}

QString TextEditorController::displayNameFor(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? QStringLiteral("document") : name;
}

QString TextEditorController::abbreviatedPath(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }
    const QString home = QDir::homePath();
    if (!home.isEmpty() && (path == home || path.startsWith(home + QLatin1Char('/')))) {
        return QStringLiteral("~") + path.mid(home.size());
    }
    return path;
}

// --- Document access -------------------------------------------------------

TextEditorController::Document *TextEditorController::activeDocument()
{
    if (m_activeIndex < 0 || m_activeIndex >= m_documents.size()) {
        return nullptr;
    }
    return &m_documents[m_activeIndex];
}

const TextEditorController::Document *TextEditorController::activeDocument() const
{
    if (m_activeIndex < 0 || m_activeIndex >= m_documents.size()) {
        return nullptr;
    }
    return &m_documents.at(m_activeIndex);
}

QVariantList TextEditorController::documents() const
{
    QVariantList list;
    list.reserve(m_documents.size());
    for (int index = 0; index < m_documents.size(); ++index) {
        const Document &document = m_documents.at(index);
        list.append(QVariantMap{
            {QStringLiteral("id"), document.id},
            {QStringLiteral("index"), index},
            {QStringLiteral("title"), document.filePath.isEmpty()
                    ? document.untitledName
                    : displayNameFor(document.filePath)},
            {QStringLiteral("filePath"), document.filePath},
            {QStringLiteral("displayPath"), abbreviatedPath(document.filePath)},
            {QStringLiteral("dirty"), document.text != document.savedText},
            {QStringLiteral("untitled"), document.filePath.isEmpty()},
            {QStringLiteral("missing"), document.missing},
        });
    }
    return list;
}

int TextEditorController::documentCount() const
{
    return m_documents.size();
}

int TextEditorController::activeIndex() const
{
    return m_activeIndex;
}

QString TextEditorController::filePath() const
{
    const Document *document = activeDocument();
    return document ? document->filePath : QString();
}

QString TextEditorController::displayPath() const
{
    const Document *document = activeDocument();
    if (!document) {
        return {};
    }
    return document->filePath.isEmpty() ? QStringLiteral("Not saved yet")
                                        : abbreviatedPath(document->filePath);
}

QString TextEditorController::documentTitle() const
{
    const Document *document = activeDocument();
    if (!document) {
        return QStringLiteral("Northstar Text Editor");
    }
    return document->filePath.isEmpty() ? document->untitledName
                                        : displayNameFor(document->filePath);
}

QString TextEditorController::text() const
{
    const Document *document = activeDocument();
    return document ? document->text : QString();
}

bool TextEditorController::dirty() const
{
    const Document *document = activeDocument();
    return document && document->text != document->savedText;
}

bool TextEditorController::canSave() const
{
    return dirty();
}

bool TextEditorController::untitled() const
{
    const Document *document = activeDocument();
    return !document || document->filePath.isEmpty();
}

bool TextEditorController::anyDirty() const
{
    return firstDirtyIndex() >= 0;
}

int TextEditorController::firstDirtyIndex() const
{
    for (int index = 0; index < m_documents.size(); ++index) {
        const Document &document = m_documents.at(index);
        if (document.text != document.savedText) {
            return index;
        }
    }
    return -1;
}

bool TextEditorController::missingOnDisk() const
{
    const Document *document = activeDocument();
    return document && document->missing;
}

bool TextEditorController::externallyModified() const
{
    const Document *document = activeDocument();
    if (!document || document->filePath.isEmpty() || document->savedSize < 0) {
        return false;
    }
    const QFileInfo info(document->filePath);
    if (!info.exists()) {
        return false;
    }
    return info.size() != document->savedSize || info.lastModified() != document->savedModified;
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

bool TextEditorController::statusIsError() const
{
    return m_statusIsError;
}

void TextEditorController::announce(const QString &message, bool error)
{
    m_statusMessage = message;
    m_statusIsError = error;
}

void TextEditorController::emitDocumentState()
{
    // Switching, opening, or reloading a document restarts find stepping from
    // the top of whatever the user is now looking at.
    m_currentMatch = -1;
    m_expectedCursor = -1;
    recomputeMatches();
    emit stateChanged();
    emit documentsChanged();
    emit findChanged();
}

// --- Loading ---------------------------------------------------------------

TextEditorController::LoadOutcome TextEditorController::readDocument(const QString &path,
                                                                     QString *contents) const
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return LoadOutcome::Missing;
    }
    if (!info.isFile()) {
        return LoadOutcome::NotText;
    }
    if (info.size() > MaximumDocumentBytes) {
        return LoadOutcome::TooLarge;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return LoadOutcome::Unreadable;
    }

    const QByteArray raw = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return LoadOutcome::Unreadable;
    }
    if (raw.contains('\0')) {
        return LoadOutcome::NotText;
    }

    QStringDecoder decoder(QStringConverter::Utf8);
    QString decoded = decoder(raw);
    if (decoder.hasError()) {
        return LoadOutcome::NotText;
    }

    if (contents) {
        *contents = decoded;
    }
    return LoadOutcome::Loaded;
}

void TextEditorController::reportLoadFailure(LoadOutcome outcome, const QString &path)
{
    const QString name = displayNameFor(path);
    switch (outcome) {
    case LoadOutcome::Missing:
        announce(QStringLiteral("%1 is no longer available at that location.").arg(name), true);
        break;
    case LoadOutcome::Unreadable:
        announce(QStringLiteral("Unable to read %1. Check that you have permission to open it.")
                     .arg(name),
                 true);
        break;
    case LoadOutcome::TooLarge:
        announce(QStringLiteral("%1 is larger than the %2 text-editor limit.")
                     .arg(name, sizeSummary(MaximumDocumentBytes)),
                 true);
        break;
    case LoadOutcome::NotText:
        announce(QStringLiteral("%1 is not a UTF-8 text file, so it cannot be edited here.")
                     .arg(name),
                 true);
        break;
    case LoadOutcome::Loaded:
        break;
    }
}

// --- Document lifecycle ----------------------------------------------------

QString TextEditorController::nextUntitledName()
{
    ++m_untitledCounter;
    return m_untitledCounter == 1
        ? QStringLiteral("Untitled")
        : QStringLiteral("Untitled %1").arg(m_untitledCounter);
}

int TextEditorController::indexOfPath(const QString &path) const
{
    for (int index = 0; index < m_documents.size(); ++index) {
        if (!path.isEmpty() && m_documents.at(index).filePath == path) {
            return index;
        }
    }
    return -1;
}

bool TextEditorController::replaceEmptyUntitledDocument() const
{
    // A single pristine "Untitled" tab is scaffolding, not a document. Opening
    // a file reuses it instead of leaving an empty tab behind.
    if (m_documents.size() != 1) {
        return false;
    }
    const Document &document = m_documents.first();
    return document.filePath.isEmpty() && document.text.isEmpty()
        && document.savedText.isEmpty();
}

void TextEditorController::appendDocument(Document document, bool activate)
{
    document.id = m_nextDocumentId++;
    m_documents.append(std::move(document));
    if (activate || m_activeIndex < 0) {
        m_activeIndex = m_documents.size() - 1;
        m_cursorPosition = 0;
    }
}

int TextEditorController::newDocument()
{
    Document document;
    document.untitledName = nextUntitledName();
    appendDocument(std::move(document), true);
    announce(QStringLiteral("Created %1.").arg(m_documents.last().untitledName));
    emitDocumentState();
    return m_activeIndex;
}

bool TextEditorController::openFile(const QString &path)
{
    const QString resolved = normalizedPath(path);
    if (resolved.isEmpty()) {
        announce(QStringLiteral("Choose a valid file to open."), true);
        emit stateChanged();
        return false;
    }

    const int existing = indexOfPath(resolved);
    if (existing >= 0) {
        activateDocument(existing);
        announce(QStringLiteral("%1 is already open.").arg(displayNameFor(resolved)));
        emit stateChanged();
        return true;
    }

    QString contents;
    const LoadOutcome outcome = readDocument(resolved, &contents);
    if (outcome != LoadOutcome::Loaded) {
        reportLoadFailure(outcome, resolved);
        if (outcome == LoadOutcome::Missing && m_recentFiles.forget(resolved)) {
            // A history entry pointing at a deleted file is noise; drop it.
            refreshRecentFiles();
            emit recentFilesChanged();
        }
        emit stateChanged();
        return false;
    }

    Document document;
    document.filePath = resolved;
    document.text = contents;
    document.savedText = contents;
    recordSavedFileState(&document, resolved);

    if (replaceEmptyUntitledDocument()) {
        document.id = m_documents.first().id;
        document.untitledName = m_documents.first().untitledName;
        m_documents[0] = std::move(document);
        m_activeIndex = 0;
        m_cursorPosition = 0;
    } else {
        appendDocument(std::move(document), true);
    }

    m_recentFiles.remember(resolved);
    refreshRecentFiles();
    announce(QStringLiteral("Opened %1.").arg(displayNameFor(resolved)));
    emit recentFilesChanged();
    emitDocumentState();
    return true;
}

void TextEditorController::activateDocument(int index)
{
    if (index < 0 || index >= m_documents.size() || index == m_activeIndex) {
        return;
    }
    m_activeIndex = index;
    m_cursorPosition = 0;

    // Surface an external deletion the moment the user returns to the tab.
    Document *document = activeDocument();
    if (document && !document->filePath.isEmpty()) {
        document->missing = !QFileInfo::exists(document->filePath);
        if (document->missing) {
            announce(QStringLiteral("%1 was removed on disk. Saving recreates it.")
                         .arg(displayNameFor(document->filePath)),
                     true);
        }
    }
    emitDocumentState();
}

bool TextEditorController::closeDocument(int index)
{
    if (index < 0 || index >= m_documents.size()) {
        return false;
    }
    const Document &document = m_documents.at(index);
    if (document.text != document.savedText) {
        announce(QStringLiteral("%1 has unsaved changes.")
                     .arg(document.filePath.isEmpty() ? document.untitledName
                                                      : displayNameFor(document.filePath)),
                 true);
        emit stateChanged();
        return false;
    }
    discardDocument(index);
    return true;
}

void TextEditorController::discardDocument(int index)
{
    if (index < 0 || index >= m_documents.size()) {
        return;
    }

    const Document &closing = m_documents.at(index);
    const QString closedName = closing.filePath.isEmpty() ? closing.untitledName
                                                          : displayNameFor(closing.filePath);
    m_documents.removeAt(index);

    if (m_documents.isEmpty()) {
        // The window always holds at least one document to type into.
        Document replacement;
        replacement.untitledName = nextUntitledName();
        appendDocument(std::move(replacement), true);
    } else if (index < m_activeIndex) {
        --m_activeIndex;
    } else if (m_activeIndex >= m_documents.size()) {
        m_activeIndex = m_documents.size() - 1;
    }

    m_cursorPosition = 0;
    announce(QStringLiteral("Closed %1.").arg(closedName));
    emitDocumentState();
}

bool TextEditorController::reloadDocument()
{
    Document *document = activeDocument();
    if (!document || document->filePath.isEmpty()) {
        announce(QStringLiteral("This document has never been saved, so there is nothing to reload."),
                 true);
        emit stateChanged();
        return false;
    }

    const QString path = document->filePath;
    QString contents;
    const LoadOutcome outcome = readDocument(path, &contents);
    if (outcome != LoadOutcome::Loaded) {
        reportLoadFailure(outcome, path);
        document->missing = outcome == LoadOutcome::Missing;
        emit stateChanged();
        return false;
    }

    document->text = contents;
    document->savedText = contents;
    recordSavedFileState(document, path);
    m_cursorPosition = 0;
    announce(QStringLiteral("Reloaded %1 from disk.").arg(displayNameFor(path)));
    emitDocumentState();
    return true;
}

// --- Saving ----------------------------------------------------------------

bool TextEditorController::writeDocument(const Document &document, const QString &path) const
{
    const QByteArray encodedText = document.text.toUtf8();
    const bool replacingExisting = QFileInfo::exists(path);
    const QFile::Permissions existingPermissions =
        replacingExisting ? QFile::permissions(path) : QFile::Permissions();

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(encodedText) != encodedText.size()) {
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        return false;
    }

    if (replacingExisting && existingPermissions != QFile::Permissions()) {
        QFile::setPermissions(path, existingPermissions);
    }
    return true;
}

void TextEditorController::recordSavedFileState(Document *document, const QString &path)
{
    const QFileInfo info(path);
    document->savedSize = info.exists() ? info.size() : -1;
    document->savedModified = info.exists() ? info.lastModified() : QDateTime();
    document->missing = !info.exists();
}

bool TextEditorController::commitSave(Document *document, const QString &path,
                                      bool allowExternalOverwrite)
{
    const QFileInfo targetInfo(path);
    if (!targetInfo.dir().exists() && !QDir().mkpath(targetInfo.absolutePath())) {
        announce(QStringLiteral("Unable to create the folder for %1.").arg(displayNameFor(path)),
                 true);
        emit stateChanged();
        return false;
    }

    const bool recreating = document->filePath == path && document->savedSize >= 0
        && !targetInfo.exists();

    if (!allowExternalOverwrite && document->filePath == path && document->savedSize >= 0
        && targetInfo.exists()
        && (targetInfo.size() != document->savedSize
            || targetInfo.lastModified() != document->savedModified)) {
        announce(QStringLiteral("%1 changed on disk since it was opened.").arg(displayNameFor(path)),
                 true);
        emit stateChanged();
        return false;
    }

    if (!writeDocument(*document, path)) {
        announce(QStringLiteral("Unable to save %1. Check that the location is writable.")
                     .arg(displayNameFor(path)),
                 true);
        emit stateChanged();
        return false;
    }

    document->filePath = path;
    document->savedText = document->text;
    recordSavedFileState(document, path);

    m_recentFiles.remember(path);
    refreshRecentFiles();
    announce(recreating ? QStringLiteral("Recreated %1.").arg(displayNameFor(path))
                        : QStringLiteral("Saved %1.").arg(displayNameFor(path)));
    emit recentFilesChanged();
    emitDocumentState();
    return true;
}

bool TextEditorController::save()
{
    Document *document = activeDocument();
    if (!document) {
        return false;
    }
    if (document->filePath.isEmpty()) {
        announce(QStringLiteral("Choose a name to save this new document."), true);
        emit stateChanged();
        return false;
    }
    return commitSave(document, document->filePath, false);
}

bool TextEditorController::saveOverwritingExternalChanges()
{
    Document *document = activeDocument();
    if (!document || document->filePath.isEmpty()) {
        return false;
    }
    return commitSave(document, document->filePath, true);
}

bool TextEditorController::saveAs(const QString &path)
{
    Document *document = activeDocument();
    if (!document) {
        return false;
    }

    const QString trimmed = path.trimmed();
    const QFileInfo info(QDir::cleanPath(QDir::fromNativeSeparators(trimmed)));
    if (trimmed.isEmpty() || !info.isAbsolute() || info.fileName().isEmpty()
        || info.fileName() == QStringLiteral(".") || info.fileName() == QStringLiteral("..")) {
        announce(QStringLiteral("Choose a valid file name."), true);
        emit stateChanged();
        return false;
    }

    const QString targetPath = info.absoluteFilePath();
    if (QFileInfo(targetPath).isDir()) {
        announce(QStringLiteral("%1 is a folder. Choose a file name.")
                     .arg(displayNameFor(targetPath)),
                 true);
        emit stateChanged();
        return false;
    }

    const int existing = indexOfPath(targetPath);
    if (existing >= 0 && existing != m_activeIndex) {
        announce(QStringLiteral("%1 is already open in another tab.")
                     .arg(displayNameFor(targetPath)),
                 true);
        emit stateChanged();
        return false;
    }

    // Save As writes the destination the user just chose; there is no earlier
    // recorded state for that path to conflict with.
    document->savedSize = -1;
    document->savedModified = QDateTime();
    return commitSave(document, targetPath, true);
}

QString TextEditorController::suggestedSavePath(const QString &fileName) const
{
    const QString trimmed = fileName.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (QFileInfo(trimmed).isAbsolute()) {
        return QDir::cleanPath(trimmed);
    }

    const Document *document = activeDocument();
    QString directory = defaultSaveDirectory();
    if (document && !document->filePath.isEmpty()) {
        directory = QFileInfo(document->filePath).absolutePath();
    } else if (!m_browsePath.isEmpty()) {
        directory = m_browsePath;
    }
    return QDir(directory).filePath(trimmed);
}

// --- Recent files ----------------------------------------------------------

void TextEditorController::refreshRecentFiles()
{
    m_recentFileEntries.clear();
    const QStringList paths = m_recentFiles.paths();
    m_recentFileEntries.reserve(paths.size());
    for (const QString &path : paths) {
        const QFileInfo info(path);
        m_recentFileEntries.append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("name"), displayNameFor(path)},
            {QStringLiteral("directory"), abbreviatedPath(info.absolutePath())},
            {QStringLiteral("available"), info.isFile() && info.isReadable()},
        });
    }
}

QVariantList TextEditorController::recentFiles() const
{
    return m_recentFileEntries;
}

bool TextEditorController::hasRecentFiles() const
{
    return !m_recentFileEntries.isEmpty();
}

bool TextEditorController::openRecent(int index)
{
    if (index < 0 || index >= m_recentFileEntries.size()) {
        return false;
    }
    const QString path = m_recentFileEntries.at(index).toMap()
                             .value(QStringLiteral("path")).toString();
    return openFile(path);
}

bool TextEditorController::forgetRecent(int index)
{
    if (index < 0 || index >= m_recentFileEntries.size()) {
        return false;
    }
    const QString path = m_recentFileEntries.at(index).toMap()
                             .value(QStringLiteral("path")).toString();
    if (!m_recentFiles.forget(path)) {
        return false;
    }
    refreshRecentFiles();
    announce(QStringLiteral("Removed %1 from Open Recent.").arg(displayNameFor(path)));
    emit recentFilesChanged();
    emit stateChanged();
    return true;
}

void TextEditorController::clearRecentFiles()
{
    m_recentFiles.clear();
    refreshRecentFiles();
    announce(QStringLiteral("Cleared the recent-file history."));
    emit recentFilesChanged();
    emit stateChanged();
}

// --- Find and replace ------------------------------------------------------

QString TextEditorController::findQuery() const
{
    return m_findQuery;
}

QString TextEditorController::replacementText() const
{
    return m_replacementText;
}

bool TextEditorController::findCaseSensitive() const
{
    return m_findCaseSensitive;
}

int TextEditorController::matchCount() const
{
    return m_matchPositions.size();
}

int TextEditorController::currentMatch() const
{
    return m_currentMatch < 0 ? 0 : m_currentMatch + 1;
}

QString TextEditorController::findSummary() const
{
    if (m_findQuery.isEmpty()) {
        return {};
    }
    if (m_matchPositions.isEmpty()) {
        return QStringLiteral("No matches");
    }
    if (m_currentMatch < 0) {
        return m_matchPositions.size() == 1
            ? QStringLiteral("1 match")
            : QStringLiteral("%1 matches").arg(m_matchPositions.size());
    }
    return QStringLiteral("%1 of %2").arg(m_currentMatch + 1).arg(m_matchPositions.size());
}

void TextEditorController::recomputeMatches()
{
    const int previousPosition = m_currentMatch >= 0 && m_currentMatch < m_matchPositions.size()
        ? m_matchPositions.at(m_currentMatch)
        : -1;

    m_matchPositions.clear();
    m_currentMatch = -1;

    const QString documentText = text();
    if (m_findQuery.isEmpty() || documentText.isEmpty()) {
        return;
    }

    const Qt::CaseSensitivity sensitivity = m_findCaseSensitive ? Qt::CaseSensitive
                                                                : Qt::CaseInsensitive;
    int from = 0;
    while (from <= documentText.size() - m_findQuery.size()) {
        const int position = documentText.indexOf(m_findQuery, from, sensitivity);
        if (position < 0) {
            break;
        }
        m_matchPositions.append(position);
        from = position + m_findQuery.size();
    }

    if (previousPosition >= 0) {
        const int restored = m_matchPositions.indexOf(previousPosition);
        if (restored >= 0) {
            m_currentMatch = restored;
        }
    }
}

void TextEditorController::setFindQuery(const QString &query)
{
    if (m_findQuery == query) {
        return;
    }
    m_findQuery = query;
    m_currentMatch = -1;
    recomputeMatches();
    emit findChanged();
}

void TextEditorController::setReplacementText(const QString &replacement)
{
    if (m_replacementText == replacement) {
        return;
    }
    m_replacementText = replacement;
    emit findChanged();
}

void TextEditorController::setFindCaseSensitive(bool caseSensitive)
{
    if (m_findCaseSensitive == caseSensitive) {
        return;
    }
    m_findCaseSensitive = caseSensitive;
    m_currentMatch = -1;
    recomputeMatches();
    emit findChanged();
}

void TextEditorController::setCursorPosition(int position)
{
    m_cursorPosition = position < 0 ? 0 : position;
    if (m_cursorPosition != m_expectedCursor) {
        // The caret moved for some reason other than a find, so stepping must
        // resume from wherever the user actually is.
        m_currentMatch = -1;
    }
}

bool TextEditorController::findNext()
{
    recomputeMatches();
    if (m_matchPositions.isEmpty()) {
        announce(m_findQuery.isEmpty() ? QStringLiteral("Enter text to find.")
                                       : QStringLiteral("No matches for \"%1\".").arg(m_findQuery),
                 true);
        emit findChanged();
        emit stateChanged();
        return false;
    }

    int target = 0;
    if (m_currentMatch >= 0 && m_currentMatch < m_matchPositions.size()) {
        target = (m_currentMatch + 1) % m_matchPositions.size();
    } else {
        for (int index = 0; index < m_matchPositions.size(); ++index) {
            if (m_matchPositions.at(index) >= m_cursorPosition) {
                target = index;
                break;
            }
        }
    }

    m_currentMatch = target;
    const int start = m_matchPositions.at(target);
    m_cursorPosition = start + m_findQuery.size();
    m_expectedCursor = m_cursorPosition;
    emit findChanged();
    emit selectionRequested(start, m_cursorPosition);
    return true;
}

bool TextEditorController::findPrevious()
{
    recomputeMatches();
    if (m_matchPositions.isEmpty()) {
        announce(m_findQuery.isEmpty() ? QStringLiteral("Enter text to find.")
                                       : QStringLiteral("No matches for \"%1\".").arg(m_findQuery),
                 true);
        emit findChanged();
        emit stateChanged();
        return false;
    }

    int target = m_matchPositions.size() - 1;
    if (m_currentMatch >= 0 && m_currentMatch < m_matchPositions.size()) {
        target = (m_currentMatch + m_matchPositions.size() - 1) % m_matchPositions.size();
    } else {
        for (int index = m_matchPositions.size() - 1; index >= 0; --index) {
            if (m_matchPositions.at(index) + m_findQuery.size() <= m_cursorPosition) {
                target = index;
                break;
            }
        }
    }

    m_currentMatch = target;
    const int start = m_matchPositions.at(target);
    m_cursorPosition = start;
    m_expectedCursor = start;
    emit findChanged();
    // Anchor after the match so the caret lands on its first character and a
    // second Find Previous steps to the match above it.
    emit selectionRequested(start + m_findQuery.size(), start);
    return true;
}

bool TextEditorController::replaceCurrent()
{
    Document *document = activeDocument();
    if (!document || m_findQuery.isEmpty()) {
        return false;
    }
    if (m_currentMatch < 0 || m_currentMatch >= m_matchPositions.size()) {
        // Nothing is selected yet: select the next match so the user always
        // sees what a second press will change.
        return findNext();
    }

    const int start = m_matchPositions.at(m_currentMatch);
    document->text.replace(start, m_findQuery.size(), m_replacementText);
    m_cursorPosition = start + m_replacementText.size();
    m_expectedCursor = m_cursorPosition;

    recomputeMatches();
    m_currentMatch = -1;
    announce(QStringLiteral("Replaced one match."));
    emit findChanged();
    emit stateChanged();
    emit documentsChanged();
    emit selectionRequested(start, m_cursorPosition);
    return true;
}

int TextEditorController::replaceAll()
{
    Document *document = activeDocument();
    if (!document || m_findQuery.isEmpty()) {
        return 0;
    }

    recomputeMatches();
    const int replaced = m_matchPositions.size();
    if (replaced == 0) {
        announce(QStringLiteral("No matches for \"%1\".").arg(m_findQuery), true);
        emit findChanged();
        emit stateChanged();
        return 0;
    }

    // Replace from the end so the earlier match offsets stay valid.
    for (int index = replaced - 1; index >= 0; --index) {
        document->text.replace(m_matchPositions.at(index), m_findQuery.size(), m_replacementText);
    }

    m_cursorPosition = 0;
    m_expectedCursor = 0;
    recomputeMatches();
    m_currentMatch = -1;
    announce(replaced == 1 ? QStringLiteral("Replaced one match.")
                           : QStringLiteral("Replaced %1 matches.").arg(replaced));
    emit findChanged();
    emit stateChanged();
    emit documentsChanged();
    return replaced;
}

// --- Open browser ----------------------------------------------------------

QString TextEditorController::browsePath() const
{
    return m_browsePath;
}

QString TextEditorController::browseDisplayPath() const
{
    return abbreviatedPath(m_browsePath);
}

QVariantList TextEditorController::browseEntries() const
{
    return m_browseEntries;
}

bool TextEditorController::browseCanNavigateUp() const
{
    if (m_browsePath.isEmpty()) {
        return false;
    }
    QDir directory(m_browsePath);
    return directory.cdUp();
}

bool TextEditorController::browseTruncated() const
{
    return m_browseTruncated;
}

bool TextEditorController::browseTo(const QString &path)
{
    const QString resolved = normalizedPath(path);
    const QFileInfo info(resolved);
    if (resolved.isEmpty() || !info.exists() || !info.isDir()) {
        announce(QStringLiteral("%1 is not a folder that can be browsed.")
                     .arg(path.trimmed().isEmpty() ? QStringLiteral("That location")
                                                   : path.trimmed()),
                 true);
        emit stateChanged();
        return false;
    }

    QDir directory(resolved);
    if (!info.isReadable() || !directory.isReadable()) {
        announce(QStringLiteral("Unable to list %1. Check that you have permission to open it.")
                     .arg(displayNameFor(resolved)),
                 true);
        emit stateChanged();
        return false;
    }

    m_browsePath = directory.absolutePath();
    refreshBrowse();
    return true;
}

bool TextEditorController::browseUp()
{
    QDir directory(m_browsePath);
    if (!directory.cdUp()) {
        return false;
    }
    return browseTo(directory.absolutePath());
}

bool TextEditorController::browseHome()
{
    return browseTo(QDir::homePath());
}

void TextEditorController::refreshBrowse()
{
    m_browseEntries.clear();
    m_browseTruncated = false;
    if (m_browsePath.isEmpty()) {
        emit browseChanged();
        return;
    }

    const QDir directory(m_browsePath);
    const QFileInfoList infos = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &info : infos) {
        if (m_browseEntries.size() >= MaximumBrowseEntries) {
            m_browseTruncated = true;
            break;
        }

        const bool isDirectory = info.isDir();
        const bool tooLarge = !isDirectory && info.size() > MaximumDocumentBytes;
        const bool openable = isDirectory
            ? info.isReadable()
            : info.isFile() && info.isReadable() && !tooLarge;

        QString reason;
        if (tooLarge) {
            reason = QStringLiteral("Larger than %1").arg(sizeSummary(MaximumDocumentBytes));
        } else if (!openable) {
            reason = QStringLiteral("Not readable");
        }

        m_browseEntries.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), info.absoluteFilePath()},
            {QStringLiteral("isDirectory"), isDirectory},
            {QStringLiteral("openable"), openable},
            {QStringLiteral("size"), isDirectory ? QString() : sizeSummary(info.size())},
            {QStringLiteral("reason"), reason},
        });
    }

    emit browseChanged();
}

// --- Editing ---------------------------------------------------------------

void TextEditorController::setText(const QString &text)
{
    Document *document = activeDocument();
    if (!document || document->text == text) {
        return;
    }

    document->text = text;
    recomputeMatches();
    emit stateChanged();
    emit documentsChanged();
    emit findChanged();
}
