#include "RudderContexts.h"
#include "Url.h"

#include <FrecencySearch/LearnedPicks.h>

#include <unordered_set>

namespace grazer
{
namespace
{
constexpr auto pageContextPrefix = std::string_view {"url:"};

template <typename Value>
std::map<std::string, Value> readCollection(emberstore::Collection<Value>& collection)
{
    auto byId = std::map<std::string, Value> {};
    auto ids = collection.ids();
    auto values = collection.values();

    for (auto index = std::size_t {0}; index < ids.size() && index < values.size(); ++index)
        byId[ids[index]] = values[index];

    return byId;
}

template <typename Value>
const Value* findRecorded(const std::map<std::string, Value>& byKey, const std::string& key)
{
    auto found = byKey.find(key);
    return found == byKey.end() ? nullptr : &found->second;
}

void addMruStamp(rudder::protocol::ContextUsage& usage, const Stamp& stamp)
{
    usage.lastUsedAtMs = stamp.at;
    usage.useCount =
        rudder::protocol::DecayingUseCount {stamp.usage, stamp.at, frecencysearch::usageHalfLifeMs};
}

void addHistory(rudder::protocol::ContextUsage& usage, const HistoryEntry& entry)
{
    usage.lastVisitAtMs = entry.lastVisit;
    usage.visitCount = entry.visits;
}

Miro::Omittable<rudder::protocol::ContextUsage> describeUsage(const std::string& url,
                                                              const RecordedPageUse& recorded)
{
    auto* stamp = findRecorded(recorded.stampsByNormalizedUrl, normalizeURL(url));
    auto* history = findRecorded(recorded.historyByPageKey, pageKey(url));

    if (stamp == nullptr && history == nullptr)
        return {};

    auto usage = rudder::protocol::ContextUsage {};

    if (stamp != nullptr)
        addMruStamp(usage, *stamp);

    if (history != nullptr)
        addHistory(usage, *history);

    return usage;
}

rudder::protocol::Context createTabContext(const RudderTab& tab, const RecordedPageUse& recorded)
{
    auto context = rudder::protocol::Context {};
    context.id = std::to_string(tab.id);
    context.title = tab.title.empty() ? tab.url : tab.title;
    context.subtitle = tab.url;
    context.url = tab.url;
    context.focused = tab.focused;
    context.usage = describeUsage(tab.url, recorded);
    return context;
}

// History keeps the url as it was visited and the page's title; a page used
// but never recorded there only has its normalized key, which navigationURL
// makes openable.
rudder::protocol::Context createUnopenedPageContext(const std::string& normalizedUrl,
                                                    const RecordedPageUse& recorded)
{
    auto* history = findRecorded(recorded.historyByPageKey, pageKey(normalizedUrl));
    auto hasTitle = history != nullptr && !history->title.empty();
    auto url = history != nullptr ? history->url : navigationURL(normalizedUrl);

    auto context = rudder::protocol::Context {};
    context.id = std::string {pageContextPrefix} + url;
    context.title = hasTitle ? history->title : normalizedUrl;
    context.subtitle = url;
    context.url = url;
    context.open = false;
    context.usage = describeUsage(url, recorded);
    return context;
}
} // namespace

RecordedPageUse readRecordedPageUse(MruStore& mru, HistoryStore& history)
{
    return {readCollection(mru.stamps), readCollection(history.entries)};
}

std::vector<rudder::protocol::Context> buildRudderContexts(const std::vector<RudderTab>& tabs,
                                                           const RecordedPageUse& recorded)
{
    auto contexts = std::vector<rudder::protocol::Context> {};
    auto shownInATab = std::unordered_set<std::string> {};

    for (auto& tab: tabs)
    {
        shownInATab.insert(normalizeURL(tab.url));
        contexts.push_back(createTabContext(tab, recorded));
    }

    for (auto& [normalizedUrl, stamp]: recorded.stampsByNormalizedUrl)
        if (!shownInATab.contains(normalizedUrl))
            contexts.push_back(createUnopenedPageContext(normalizedUrl, recorded));

    return contexts;
}

std::optional<std::string> parseRudderPageContextId(const std::string& id)
{
    if (!id.starts_with(pageContextPrefix) || id.size() == pageContextPrefix.size())
        return std::nullopt;

    return id.substr(pageContextPrefix.size());
}
} // namespace grazer
