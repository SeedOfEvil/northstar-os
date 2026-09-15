#pragma once
#include <atomic>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QDateTime>
#include <memory>
#include <limits>

struct FileImportState {
    std::atomic<bool> cancelled{false};
    std::atomic<qint64> total{0};
    std::atomic<qint64> copied{0};
};

// Writes are confined to a private staging directory. Publish only a complete
// copy, without replacing an existing destination; cancellation removes staging.
inline bool importFileTree(const QString &source, const QString &destination,
                           const std::shared_ptr<FileImportState> &state)
{
    struct Entry { QString path, relative; qint64 size; QDateTime modified; bool directory; };
    QList<Entry> plan;
    const auto enumerate = [&](auto &&self, const QString &path, const QString &relative, int depth) -> bool {
        if (state->cancelled || depth > 64 || plan.size() >= 100000) return false;
        QFileInfo info(path);
        if (info.isSymLink() || (!info.isFile() && !info.isDir()) || !info.isReadable()
            || info.canonicalFilePath() != info.absoluteFilePath()) return false;
        plan.append({path, relative, info.isFile() ? info.size() : 0, info.lastModified(), info.isDir()});
        if (info.isFile()) {
            if (info.size() > std::numeric_limits<qint64>::max() - state->total.load()) return false;
            state->total += info.size();
        }
        else for (const auto &child : QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
            if (!self(self, child.absoluteFilePath(), relative + "/" + child.fileName(), depth + 1)) return false;
        return true;
    };
    if (!enumerate(enumerate, source, "payload", 0)) return false;
    const QString parent = QFileInfo(destination).absolutePath();
    if (QFileInfo(parent).canonicalFilePath() != parent) return false;
    QTemporaryDir stage(QDir(parent).filePath(".northstar-import-XXXXXX"));
    if (!stage.isValid()) return false;
    for (const auto &entry : plan) {
        if (state->cancelled) return false;
        QFileInfo current(entry.path);
        if (current.isSymLink() || current.canonicalFilePath() != entry.path
            || current.isDir() != entry.directory || current.lastModified() != entry.modified
            || (!entry.directory && current.size() != entry.size)) return false;
        const QString target = QDir(stage.path()).filePath(entry.relative);
        if (entry.directory) { if (!QDir().mkdir(target)) return false; continue; }
        QFile input(entry.path), output(target);
        if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return false;
        qint64 remaining = entry.size;
        while (remaining > 0) {
            if (state->cancelled) return false;
            const QByteArray bytes = input.read(qMin<qint64>(remaining, 1024 * 1024));
            if (bytes.isEmpty() || output.write(bytes) != bytes.size()) return false;
            remaining -= bytes.size();
            state->copied += bytes.size();
        }
        if (!input.atEnd() || !output.flush()) return false;
        const QFileInfo after(entry.path);
        if (after.size() != entry.size || after.lastModified() != entry.modified
            || after.canonicalFilePath() != entry.path) return false;
    }
    if (state->cancelled || QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) return false;
    return QFile::rename(QDir(stage.path()).filePath("payload"), destination);
}
