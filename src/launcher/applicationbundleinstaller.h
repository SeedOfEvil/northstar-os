#pragma once

#include <QObject>
#include <QString>

class ApplicationBundleInstaller final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool error READ error NOTIFY statusChanged)

public:
    explicit ApplicationBundleInstaller(QString applicationRoot = {},
                                        QString trashRoot = {},
                                        QObject *parent = nullptr);

    QString statusMessage() const;
    bool error() const;

    Q_INVOKABLE bool installBundle(const QString &sourcePath);
    Q_INVOKABLE bool removeBundle(const QString &bundleIdentifier);

    static QString defaultApplicationRoot();
    static QString defaultTrashRoot();

signals:
    void statusChanged();
    void applicationsChanged();

private:
    bool ensurePrivateDirectory(const QString &path);
    bool validateTree(const QString &path, uint ownerId, QString *reason) const;
    bool copyTree(const QString &source,
                  const QString &destination,
                  uint ownerId,
                  qsizetype *entries,
                  qint64 *bytes,
                  QString *reason) const;
    void setStatus(const QString &message, bool isError);

    QString m_applicationRoot;
    QString m_trashRoot;
    QString m_statusMessage;
    bool m_error = false;
};
