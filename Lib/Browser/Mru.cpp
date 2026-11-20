#include "Mru.h"
#include <GrazerPalette/Url.h>

#include <GrazerPalette/RankingMaterials.h>

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

namespace grazer
{
MruStore::MruStore(const emberstore::Database& db)
    : stamps(db.collection<Stamp>("mru"))
{
    prune();
}

void MruStore::touch(const std::string& normalizedUrl, std::int64_t atMs)
{
    if (isHomeURL(normalizedUrl))
        return;

    stamps.doc(normalizedUrl).set({atMs, usage(normalizedUrl, atMs) + 1});
}

double MruStore::usage(const std::string& normalizedUrl, std::int64_t atMs)
{
    if (auto stamp = stamps.get(normalizedUrl))
        return calculateStampUsage(*stamp, atMs);

    return 0;
}

std::unordered_map<std::string, double> MruStore::listUsage(std::int64_t atMs)
{
    auto usages = std::unordered_map<std::string, double> {};
    auto ids = stamps.ids();
    auto values = stamps.values();

    for (auto index = std::size_t {0}; index < ids.size(); ++index)
        usages[ids[index]] = calculateStampUsage(values[index], atMs);

    return usages;
}

std::int64_t MruStore::lastUsed(const std::string& normalizedUrl)
{
    if (auto stamp = stamps.get(normalizedUrl))
        return stamp->at;

    return 0;
}

void MruStore::prune()
{
    if (stamps.size() <= maxEntries)
        return;

    auto aged = std::vector<std::pair<std::int64_t, std::string>> {};

    for (auto& id: stamps.ids())
        aged.emplace_back(lastUsed(id), id);

    std::sort(aged.begin(), aged.end());

    auto excess = aged.size() - maxEntries;

    for (std::size_t i = 0; i < excess; ++i)
        stamps.remove(aged[i].second);
}

std::int64_t MruStore::nowMs()
{
    using namespace std::chrono;
    return (std::int64_t) duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}
} // namespace grazer
