#pragma once

#include <functional>

#include <QObject>
#include <QUrl>
#include <QVariantList>

class FileBrowserController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    using OpenFunction = std::function<bool(const QUrl &url)>;

    explicit FileBrowserController(QObject *parent = nullptr,
                                   QString rootPath = {},
                                   OpenFunction openFunction = {});

    QVariantList entries() const;
    QString currentPath() const;
    QString displayPath() const;
    QString homePath() const;
    QString errorMessage() const;

    Q_INVOKABLE bool navigateTo(const QString &path);
    Q_INVOKABLE bool navigateUp();
    Q_INVOKABLE bool goHome();
    Q_INVOKABLE bool openEntry(const QString &path);
    Q_INVOKABLE void refresh();

signals:
    void entriesChanged();
    void currentPathChanged();
    void errorMessageChanged();

private:
    static QString normalizedPath(const QString &path);
    static QString canonicalOrNormalizedPath(const QString &path);
    QString resolvePath(const QString &path) const;
    bool isWithinRoot(const QString &path) const;
    void setErrorMessage(const QString &message);

    QString m_rootPath;
    QString m_currentPath;
    QVariantList m_entries;
    OpenFunction m_openFunction;
    QString m_errorMessage;
};
