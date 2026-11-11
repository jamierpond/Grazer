#include "History.h"
#include "Url.h"

#include <algorithm>
#include <array>
#include <utility>

namespace grazer
{
namespace
{
constexpr std::array loopbackHosts = {"localhost", "127.0.0.1", "[::1]"};

std::string getHost(const std::string& url)
{
    auto normalized = normalizeURL(url);
    auto hostAndPort = normalized.substr(0, normalized.find('/'));

    if (hostAndPort.starts_with("["))
        return hostAndPort.substr(0, hostAndPort.find(']') + 1);

    return hostAndPort.substr(0, hostAndPort.find(':'));
}

// Sign-in flows bounce through a local callback server (127.0.0.1:port/callback);
// those pages are never worth suggesting again.
bool isLoopbackURL(const std::string& url)
{
    auto host = getHost(url);
    return std::find(loopbackHosts.begin(), loopbackHosts.end(), host)
           != loopbackHosts.end();
}
} // namespace

HistoryStore::HistoryStore(const emberstore::Database& db)
    : entries(db.collection<HistoryEntry>("history"))
{
    prune();
}

void HistoryStore::recordVisit(const std::string& url,
                               const std::string& title,
                               std::int64_t atMs)
{
    if (isLoopbackURL(url))
        return;

    auto ref = entries.doc(normalizeURL(url));
    auto entry = ref.get().value_or(HistoryEntry {});

    entry.url = url;

    if (!title.empty())
        entry.title = title;

    entry.lastVisit = atMs;
    entry.visits++;
    ref.set(entry);
}

void HistoryStore::updateTitle(const std::string& url, const std::string& title)
{
    if (title.empty())
        return;

    auto ref = entries.doc(normalizeURL(url));

    if (auto entry = ref.get())
    {
        entry->title = title;
        ref.set(*entry);
    }
}

void HistoryStore::forget(const std::string& url)
{
    entries.remove(normalizeURL(url));
}

std::vector<HistoryEntry> HistoryStore::all()
{
    return entries.values();
}

void HistoryStore::prune()
{
    if (entries.size() <= maxEntries)
        return;

    auto aged = std::vector<std::pair<std::int64_t, std::string>> {};

    for (auto& id: entries.ids())
    {
        auto entry = entries.get(id);
        aged.push_back({entry ? entry->lastVisit : 0, id});
    }

    std::sort(aged.begin(), aged.end());

    auto excess = aged.size() - maxEntries;

    for (std::size_t i = 0; i < excess; ++i)
        entries.remove(aged[i].second);
}
} // namespace grazer
