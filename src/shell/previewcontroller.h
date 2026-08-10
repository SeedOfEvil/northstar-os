#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class PreviewController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY previewChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY previewChanged)
    Q_PROPERTY(QString path READ path NOTIFY previewChanged)
    Q_PROPERTY(QString title READ title NOTIFY previewChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY previewChanged)
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY previewChanged)
    Q_PROPERTY(QString textContent READ textContent NOTIFY previewChanged)
    Q_PROPERTY(QString imageDataUrl READ imageDataUrl NOTIFY previewChanged)
    Q_PROPERTY(QStringList details READ details NOTIFY previewChanged)
    Q_PROPERTY(QString message READ message NOTIFY previewChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY previewChanged)

public:
    explicit PreviewController(QObject *parent = nullptr, QString homePath = {});

    QString status() const;
    QString kind() const;
    QString path() const;
    QString title() const;
    QString subtitle() const;
    QString mimeType() const;
    QString textContent() const;
    QString imageDataUrl() const;
    QStringList details() const;
    QString message() const;
    bool truncated() const;

    Q_INVOKABLE bool previewPath(const QString &path, const QString &navigationRoot = {});
    Q_INVOKABLE void clear();

signals:
    void previewChanged();

private:
    static QString canonicalOrNormalizedPath(const QString &path);
    static bool pathMatchesRoot(const QString &path, const QString &root);
    static QString formattedSize(qint64 bytes);
    static QString formattedTimestamp(const QString &path);
    bool isAllowedPath(const QString &path, const QString &navigationRoot) const;
    bool isExplicitMountedRoot(const QString &root) const;
    void previewDirectory(const QString &path);
    void previewFile(const QString &path);
    void setMetadataOnly(const QString &reason = {});
    void setError(const QString &title, const QString &message);
    void resetValues();

    QString m_homePath;
    QString m_status = QStringLiteral("empty");
    QString m_kind = QStringLiteral("metadata");
    QString m_path;
    QString m_title;
    QString m_subtitle;
    QString m_mimeType;
    QString m_textContent;
    QString m_imageDataUrl;
    QStringList m_details;
    QString m_message;
    bool m_truncated = false;
};
