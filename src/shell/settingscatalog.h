#pragma once

#include <functional>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

// One searchable registry for every real desktop setting.
//
// The catalog holds no state of its own. Each entry names the controller
// accessor that actually reads, writes, or performs the setting, so Settings
// cannot drift away from the behavior it claims to expose. An entry whose
// backing capability is missing reports that it is unavailable together with
// the reason its own controller gave, rather than presenting a control that
// silently does nothing.
class SettingsCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sections READ sections NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY resultsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY resultsChanged)
    Q_PROPERTY(QString selectedSection READ selectedSection WRITE setSelectedSection NOTIFY resultsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY resultsChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY resultsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

public:
    // A setting's presentation and the accessors that back it.
    struct Entry
    {
        QString id;
        QString section;
        QString title;
        QString description;
        QStringList keywords;
        QString kind;          // toggle, slider, choice, path, action, or info
        QString actionLabel;   // action entries only
        QString unit;          // slider entries only
        int minimum = 0;
        int maximum = 100;
        bool destructive = false;

        // choice entries only: the values the setting accepts, each as
        // {value, label}. A choice with no options is refused, because it
        // would present a control the user cannot answer.
        QVariantList options;

        // path entries only: what the file dialog should offer, and the empty
        // state to show when nothing is chosen.
        QStringList nameFilters;
        QString emptyLabel;

        std::function<QVariant()> read;
        std::function<bool(const QVariant &value)> write;
        std::function<bool()> perform;
        std::function<bool()> available;
        std::function<QString()> unavailableReason;

        // Why a write the controller accepted as well-formed was still
        // refused. Without this the surface can only say a setting "could not
        // be changed", when the controller already knows exactly why.
        std::function<QString()> writeFailureReason;
    };

    static QString toggleKind();
    static QString sliderKind();
    static QString choiceKind();
    static QString pathKind();
    static QString actionKind();
    static QString infoKind();

    // One option for a choice entry, in the shape describeEntry hands to QML.
    static QVariantMap choiceOption(const QString &value, const QString &label);

    explicit SettingsCatalog(QObject *parent = nullptr);

    void registerSection(const QString &id, const QString &label, const QString &description = {});
    bool registerEntry(Entry entry);

    QVariantList sections() const;
    QVariantList entries() const;
    QString query() const;
    QString selectedSection() const;
    bool searching() const;
    int resultCount() const;
    int entryCount() const;
    QString statusMessage() const;
    bool statusIsError() const;

    Q_INVOKABLE QVariantMap entryFor(const QString &id) const;
    Q_INVOKABLE bool setValue(const QString &id, const QVariant &value);
    Q_INVOKABLE bool invoke(const QString &id);
    Q_INVOKABLE void clearQuery();
    Q_INVOKABLE void refresh();

public slots:
    void setQuery(const QString &query);
    void setSelectedSection(const QString &section);

signals:
    void resultsChanged();
    void statusChanged();

private:
    struct Section
    {
        QString id;
        QString label;
        QString description;
    };

    static QStringList searchTokens(const QString &text);

    const Entry *findEntry(const QString &id) const;
    int scoreEntry(const Entry &entry, const QStringList &tokens) const;
    QVariantMap describeEntry(const Entry &entry) const;
    QString sectionLabel(const QString &id) const;
    bool entryAvailable(const Entry &entry) const;
    void announce(const QString &message, bool error = false);

    QList<Section> m_sections;
    QList<Entry> m_entries;
    QString m_query;
    QString m_selectedSection;
    QString m_statusMessage;
    bool m_statusIsError = false;
};
