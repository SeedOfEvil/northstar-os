#pragma once

#include <QAbstractListModel>
#include <QStringList>

class PinnedApplicationModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList desktopIds READ desktopIds NOTIFY desktopIdsChanged)
    Q_PROPERTY(int count READ count NOTIFY desktopIdsChanged)

public:
    enum Role {
        DesktopIdRole = Qt::UserRole + 1,
    };
    Q_ENUM(Role)

    explicit PinnedApplicationModel(QObject *parent = nullptr, QString settingsPath = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QStringList desktopIds() const;

    Q_INVOKABLE bool isPinned(const QString &desktopId) const;
    Q_INVOKABLE bool pin(const QString &desktopId);
    Q_INVOKABLE bool unpin(const QString &desktopId);
    Q_INVOKABLE bool movePinned(int from, int to);

signals:
    void desktopIdsChanged();

private:
    static QString normalizedDesktopId(const QString &desktopId);
    void load();
    void save() const;

    QString m_settingsPath;
    QStringList m_desktopIds;
};
