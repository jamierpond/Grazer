#include <Palette.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>

using namespace nano;
using namespace grazer;

namespace
{
using Clock = std::chrono::steady_clock;

// The history store's cap: a full history is what the palette searches on every key.
constexpr auto historyEntryCount = 2000;

// Each keystroke ranks the whole history; the rest of the 8 ms frame is drawing.
// Generous so Debug builds and CI runners pass; it catches going back to O(n) disk reads.
constexpr auto rankingBudgetMs = 25.0;

constexpr auto attemptsPerQuery = 3;

std::string makeHistoryURL(int index)
{
    return "https://www.example" + std::to_string(index % 97) + ".com/articles/2026/some-long-path/"
           + std::to_string(index) + "?utm_source=newsletter&ref=" + std::to_string(index * 7);
}

std::vector<HistoryEntry> makeFullHistory()
{
    auto history = std::vector<HistoryEntry> {};

    for (auto index = 0; index < historyEntryCount; ++index)
        history.push_back({makeHistoryURL(index), "An ordinary page title " + std::to_string(index), index, 1});

    return history;
}

double measureFastestRankingMs(const std::vector<HistoryEntry>& history, const std::string& query)
{
    auto usages = std::unordered_map<std::string, double> {};
    auto lookUpUsage = [&usages](const std::string& url)
    {
        auto found = usages.find(url);
        return found != usages.end() ? found->second : 0.0;
    };

    auto fastestMs = rankingBudgetMs * 1000.0;

    for (auto attempt = 0; attempt < attemptsPerQuery; ++attempt)
    {
        auto start = Clock::now();
        rankPalette({}, history, {query, PaletteMode::goFromHere, lookUpUsage});
        auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        fastestMs = std::min(fastestMs, elapsed);
    }

    return fastestMs;
}
} // namespace

auto tRankingFullHistoryFitsBudget = test("PaletteSpeed/ranking a full history fits a keystroke's budget") = []
{
    auto history = makeFullHistory();

    for (auto query: {"e", "ex", "exa", "page", "news"})
        check(measureFastestRankingMs(history, query) < rankingBudgetMs);
};
