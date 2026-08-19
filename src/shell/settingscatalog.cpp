#include "settingscatalog.h"

#include <algorithm>
#include <utility>

namespace {

constexpr int TitleExactScore = 1000;
constexpr int TitlePrefixScore = 400;
constexpr int TitleWordScore = 300;
constexpr int TitleWordPrefixScore = 220;
constexpr int KeywordWordScore = 260;
constexpr int KeywordWordPrefixScore = 150;
constexpr int SectionWordScore = 90;
constexpr int DescriptionWordScore = 60;

// Matching runs on whole words rather than raw substrings. Substring matching
// finds "out" inside "about" and ranks unrelated entries above the one the
// user meant.
QStringList words(const QString &text)
{
    QStringList result;
    QString current;
    for (const QChar character : text) {
        if (character.isLetterOrNumber()) {
            current.append(character.toLower());
        } else if (!current.isEmpty()) {
            result.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) {
        result.append(current);
    }
    return result;
}

int wordScore(const QStringList &candidates, const QString &token, int exactScore,
              int prefixScore)
{
    int best = 0;
    for (const QString &candidate : candidates) {
        if (candidate == token) {
            best = std::max(best, exactScore);
        } else if (candidate.startsWith(token)) {
            best = std::max(best, prefixScore);
        }
    }
    return best;
}

} // namespace

QString SettingsCatalog::toggleKind()
{
    return QStringLiteral("toggle");
}

QString SettingsCatalog::sliderKind()
{
    return QStringLiteral("slider");
}

QString SettingsCatalog::choiceKind()
{
    return QStringLiteral("choice");
}

QString SettingsCatalog::pathKind()
{
    return QStringLiteral("path");
}

QVariantMap SettingsCatalog::choiceOption(const QString &value, const QString &label)
{
    return QVariantMap{{QStringLiteral("value"), value},
                       {QStringLiteral("label"), label.isEmpty() ? value : label}};
}

QString SettingsCatalog::actionKind()
{
    return QStringLiteral("action");
}

QString SettingsCatalog::infoKind()
{
    return QStringLiteral("info");
}

SettingsCatalog::SettingsCatalog(QObject *parent)
    : QObject(parent)
{
}

void SettingsCatalog::registerSection(const QString &id, const QString &label,
                                      const QString &description)
{
    const QString trimmedId = id.trimmed();
    if (trimmedId.isEmpty()) {
        return;
    }
    for (Section &section : m_sections) {
        if (section.id == trimmedId) {
            section.label = label;
            section.description = description;
            return;
        }
    }
    m_sections.append(Section{trimmedId, label, description});
    if (m_selectedSection.isEmpty()) {
        m_selectedSection = trimmedId;
    }
    emit resultsChanged();
}

bool SettingsCatalog::registerEntry(Entry entry)
{
    entry.id = entry.id.trimmed();
    entry.section = entry.section.trimmed();
    if (entry.id.isEmpty() || entry.section.isEmpty() || entry.title.trimmed().isEmpty()) {
        return false;
    }
    if (findEntry(entry.id)) {
        return false;
    }

    // A registered control must be able to do what its kind promises,
    // otherwise Settings would show a dead control.
    if (entry.kind == toggleKind() || entry.kind == sliderKind() || entry.kind == pathKind()) {
        if (!entry.read || !entry.write) {
            return false;
        }
    } else if (entry.kind == choiceKind()) {
        if (!entry.read || !entry.write) {
            return false;
        }
        // A choice with nothing to choose from is a dead control by another
        // name, and every option has to carry a value the write accessor can
        // actually be given. A dynamic source is held to the same standard by
        // asking it now: one that cannot answer at registration would render
        // an empty control.
        const QVariantList declared = optionsFor(entry);
        if (declared.isEmpty()) {
            return false;
        }
        for (const QVariant &option : std::as_const(declared)) {
            if (option.toMap().value(QStringLiteral("value")).toString().isEmpty()) {
                return false;
            }
        }
    } else if (entry.kind == actionKind()) {
        if (!entry.perform) {
            return false;
        }
    } else if (entry.kind == infoKind()) {
        if (!entry.read) {
            return false;
        }
    } else {
        return false;
    }

    if (entry.kind == sliderKind() && entry.minimum >= entry.maximum) {
        return false;
    }

    bool sectionKnown = false;
    for (const Section &section : m_sections) {
        if (section.id == entry.section) {
            sectionKnown = true;
            break;
        }
    }
    if (!sectionKnown) {
        return false;
    }

    m_entries.append(std::move(entry));
    emit resultsChanged();
    return true;
}

QVariantList SettingsCatalog::optionsFor(const Entry &entry)
{
    return entry.optionSource ? entry.optionSource() : entry.options;
}

const SettingsCatalog::Entry *SettingsCatalog::findEntry(const QString &id) const
{
    for (const Entry &entry : m_entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QString SettingsCatalog::sectionLabel(const QString &id) const
{
    for (const Section &section : m_sections) {
        if (section.id == id) {
            return section.label;
        }
    }
    return id;
}

bool SettingsCatalog::entryAvailable(const Entry &entry) const
{
    return entry.available ? entry.available() : true;
}

QStringList SettingsCatalog::searchTokens(const QString &text)
{
    QStringList tokens;
    const QStringList parts = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString token = part.trimmed().toLower();
        if (!token.isEmpty()) {
            tokens.append(token);
        }
    }
    return tokens;
}

int SettingsCatalog::scoreEntry(const Entry &entry, const QStringList &tokens) const
{
    if (tokens.isEmpty()) {
        return 0;
    }

    const QString title = entry.title.toLower();
    const QStringList titleWords = words(entry.title);
    const QStringList descriptionWords = words(entry.description);
    const QStringList sectionWords = words(sectionLabel(entry.section));

    // A multi-word keyword such as "log out" has to be reachable by either of
    // its words, so keywords are matched word by word too.
    QStringList keywordWords;
    for (const QString &keyword : entry.keywords) {
        keywordWords.append(words(keyword));
    }

    int total = 0;
    for (const QString &token : tokens) {
        int best = 0;

        if (title == token) {
            best = std::max(best, TitleExactScore);
        } else if (title.startsWith(token)) {
            best = std::max(best, TitlePrefixScore);
        }

        best = std::max(best, wordScore(titleWords, token, TitleWordScore, TitleWordPrefixScore));
        best = std::max(best,
                        wordScore(keywordWords, token, KeywordWordScore, KeywordWordPrefixScore));
        best = std::max(best, wordScore(sectionWords, token, SectionWordScore, SectionWordScore));
        best = std::max(best,
                        wordScore(descriptionWords, token, DescriptionWordScore,
                                  DescriptionWordScore));

        // Every token must match something, so "dark files" does not return
        // every entry that merely mentions one of the two words.
        if (best == 0) {
            return 0;
        }
        total += best;
    }

    // An unavailable control is still worth finding, but a working one that
    // matches equally well should be offered first.
    if (!entryAvailable(entry)) {
        total /= 2;
    }
    return total;
}

QVariantMap SettingsCatalog::describeEntry(const Entry &entry) const
{
    const bool available = entryAvailable(entry);
    QString reason;
    if (!available && entry.unavailableReason) {
        reason = entry.unavailableReason();
    }

    QVariantMap map{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("section"), entry.section},
        {QStringLiteral("sectionLabel"), sectionLabel(entry.section)},
        {QStringLiteral("title"), entry.title},
        {QStringLiteral("description"), entry.description},
        {QStringLiteral("kind"), entry.kind},
        {QStringLiteral("actionLabel"), entry.actionLabel.isEmpty() ? entry.title
                                                                   : entry.actionLabel},
        {QStringLiteral("unit"), entry.unit},
        {QStringLiteral("minimum"), entry.minimum},
        {QStringLiteral("maximum"), entry.maximum},
        {QStringLiteral("destructive"), entry.destructive},
        {QStringLiteral("available"), available},
        {QStringLiteral("unavailableReason"), reason},
        {QStringLiteral("writable"), entry.kind == toggleKind() || entry.kind == sliderKind()
                                         || entry.kind == choiceKind()
                                         || entry.kind == pathKind()},
        {QStringLiteral("options"), optionsFor(entry)},
        {QStringLiteral("allowsUnset"), entry.allowsUnset},
        {QStringLiteral("nameFilters"), entry.nameFilters},
        {QStringLiteral("emptyLabel"), entry.emptyLabel},
    };
    map.insert(QStringLiteral("value"), entry.read ? entry.read() : QVariant());
    return map;
}

QVariantList SettingsCatalog::sections() const
{
    QVariantList list;
    list.reserve(m_sections.size());
    for (const Section &section : m_sections) {
        int count = 0;
        for (const Entry &entry : m_entries) {
            if (entry.section == section.id) {
                ++count;
            }
        }
        list.append(QVariantMap{
            {QStringLiteral("id"), section.id},
            {QStringLiteral("label"), section.label},
            {QStringLiteral("description"), section.description},
            {QStringLiteral("count"), count},
        });
    }
    return list;
}

QVariantList SettingsCatalog::entries() const
{
    QVariantList list;

    if (!searching()) {
        for (const Entry &entry : m_entries) {
            if (entry.section == m_selectedSection) {
                list.append(describeEntry(entry));
            }
        }
        return list;
    }

    // A search deliberately crosses every section: the point of searching is
    // to find a setting without knowing where it lives.
    const QStringList tokens = searchTokens(m_query);
    QList<QPair<int, int>> ranked;
    for (int index = 0; index < m_entries.size(); ++index) {
        const int score = scoreEntry(m_entries.at(index), tokens);
        if (score > 0) {
            ranked.append({score, index});
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const QPair<int, int> &left, const QPair<int, int> &right) {
                         return left.first > right.first;
                     });

    list.reserve(ranked.size());
    for (const QPair<int, int> &match : ranked) {
        list.append(describeEntry(m_entries.at(match.second)));
    }
    return list;
}

QString SettingsCatalog::query() const
{
    return m_query;
}

QString SettingsCatalog::selectedSection() const
{
    return m_selectedSection;
}

bool SettingsCatalog::searching() const
{
    return !m_query.trimmed().isEmpty();
}

int SettingsCatalog::resultCount() const
{
    return entries().size();
}

int SettingsCatalog::entryCount() const
{
    return m_entries.size();
}

QString SettingsCatalog::statusMessage() const
{
    return m_statusMessage;
}

bool SettingsCatalog::statusIsError() const
{
    return m_statusIsError;
}

void SettingsCatalog::announce(const QString &message, bool error)
{
    m_statusMessage = message;
    m_statusIsError = error;
    emit statusChanged();
}

QVariantMap SettingsCatalog::entryFor(const QString &id) const
{
    const Entry *entry = findEntry(id);
    return entry ? describeEntry(*entry) : QVariantMap();
}

void SettingsCatalog::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    emit resultsChanged();
}

void SettingsCatalog::clearQuery()
{
    setQuery(QString());
}

void SettingsCatalog::setSelectedSection(const QString &section)
{
    if (m_selectedSection == section) {
        return;
    }
    for (const Section &candidate : m_sections) {
        if (candidate.id == section) {
            m_selectedSection = section;
            emit resultsChanged();
            return;
        }
    }
}

void SettingsCatalog::refresh()
{
    emit resultsChanged();
}

bool SettingsCatalog::setValue(const QString &id, const QVariant &value)
{
    const Entry *entry = findEntry(id);
    if (!entry) {
        announce(QStringLiteral("That setting is not available in this build."), true);
        return false;
    }
    if (entry->kind != toggleKind() && entry->kind != sliderKind()
        && entry->kind != choiceKind() && entry->kind != pathKind()) {
        announce(QStringLiteral("%1 cannot be changed here.").arg(entry->title), true);
        return false;
    }
    if (!entryAvailable(*entry)) {
        const QString reason = entry->unavailableReason ? entry->unavailableReason() : QString();
        announce(reason.isEmpty()
                     ? QStringLiteral("%1 is unavailable on this system.").arg(entry->title)
                     : QStringLiteral("%1 is unavailable: %2").arg(entry->title, reason),
                 true);
        return false;
    }

    QVariant requested = value;
    if (entry->kind == sliderKind()) {
        bool converted = false;
        const int number = value.toInt(&converted);
        if (!converted) {
            announce(QStringLiteral("%1 needs a numeric value.").arg(entry->title), true);
            return false;
        }
        requested = std::clamp(number, entry->minimum, entry->maximum);
    } else if (entry->kind == choiceKind()) {
        // The catalog owns the option list, so it is the catalog that rejects
        // a value not on it. Anything further is the controller's business.
        const QString candidate = value.toString();
        bool offered = false;
        const QVariantList available = optionsFor(*entry);
        for (const QVariant &option : std::as_const(available)) {
            if (option.toMap().value(QStringLiteral("value")).toString() == candidate) {
                offered = true;
                break;
            }
        }
        if (!offered) {
            announce(QStringLiteral("%1 does not offer that choice.").arg(entry->title), true);
            return false;
        }
        requested = candidate;
    } else if (entry->kind == pathKind()) {
        // Whether a path is usable depends on the file behind it, which only
        // the controller can judge. Pass it through and let the refusal come
        // back with a reason.
        requested = value.toString().trimmed();
    } else {
        requested = value.toBool();
    }

    if (!entry->write(requested)) {
        const QString why = entry->writeFailureReason ? entry->writeFailureReason() : QString();
        announce(why.isEmpty() ? QStringLiteral("%1 could not be changed.").arg(entry->title) : why,
                 true);
        return false;
    }

    announce(QStringLiteral("%1 updated.").arg(entry->title));
    emit resultsChanged();
    return true;
}

bool SettingsCatalog::invoke(const QString &id)
{
    const Entry *entry = findEntry(id);
    if (!entry) {
        announce(QStringLiteral("That action is not available in this build."), true);
        return false;
    }
    if (entry->kind != actionKind()) {
        announce(QStringLiteral("%1 is not an action.").arg(entry->title), true);
        return false;
    }
    if (!entryAvailable(*entry)) {
        const QString reason = entry->unavailableReason ? entry->unavailableReason() : QString();
        announce(reason.isEmpty()
                     ? QStringLiteral("%1 is unavailable on this system.").arg(entry->title)
                     : QStringLiteral("%1 is unavailable: %2").arg(entry->title, reason),
                 true);
        return false;
    }

    if (!entry->perform()) {
        announce(QStringLiteral("%1 did not complete.").arg(entry->title), true);
        return false;
    }

    announce(QStringLiteral("%1 completed.").arg(entry->title));
    emit resultsChanged();
    return true;
}
