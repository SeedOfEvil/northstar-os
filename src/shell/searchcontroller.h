#pragma once

#include <atomic>
#include <memory>

#include <QObject>
#include <QTimer>
#include <QVariantList>

class ApplicationLauncher;

class SearchController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)

public:
    explicit SearchController(ApplicationLauncher *launcher,
                              QObject *parent = nullptr,
                              QString homePath = {});

    QString query() const;
    QVariantList results() const;
    bool searching() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool activateResult(int index);

public slots:
    void setQuery(const QString &query);

signals:
    void queryChanged();
    void resultsChanged();
    void searchingChanged();
    void actionRequested(const QString &actionId);
    void applicationRequested(const QString &desktopId);
    void fileRequested(const QString &path, bool isDirectory);

private:
    struct FileSearchResult {
        QVariantList results;
        bool truncated = false;
    };

    static FileSearchResult searchHome(const QString &homePath,
                                       const QString &query,
                                       const std::shared_ptr<std::atomic_bool> &cancelled);
    static QString applicationIcon(const QVariantMap &application);
    static bool matchesTerms(const QString &candidate, const QStringList &terms);
    static bool pathWithin(const QString &path, const QString &root);
    void rebuildImmediateResults();
    void beginFileSearch();
    void setSearching(bool searching);

    ApplicationLauncher *m_launcher = nullptr;
    QString m_homePath;
    QString m_query;
    QVariantList m_immediateResults;
    QVariantList m_results;
    QTimer m_debounceTimer;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    quint64 m_generation = 0;
    bool m_searching = false;
};
