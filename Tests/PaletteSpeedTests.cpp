#include <Palette.h>

#include <FrecencySearch/MatchLevel.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <unordered_map>

using namespace nano;
using namespace grazer;

namespace
{
using Clock = std::chrono::steady_clock;

// The history store's cap: a full history is what the palette searches on every key.
constexpr auto historyEntryCount = 2000;

// Ranking may cost this many times a plain pass that lower-cases every entry's
// title and URL, timed in the same run: a slow CI runner slows both alike.
// Measured locally at about 13 in Debug (what `just test` and CI build), 3.5 in
// Release; room for a noisy runner, while a 10x slowdown still fails.
constexpr auto rankingBudgetInBaselinePasses = 40.0;

constexpr auto attemptsPerMeasurement = 5;

template <typename Work>
double measureFastestMs(Work&& work)
{
    auto fastestMs = std::numeric_limits<double>::max();

    for (auto attempt = 0; attempt < attemptsPerMeasurement; ++attempt)
    {
        auto start = Clock::now();
        work();
        auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        fastestMs = std::min(fastestMs, elapsed);
    }

    return fastestMs;
}

double measureBaselinePassMs(const std::vector<HistoryEntry>& history)
{
    return measureFastestMs(
        [&]
        {
            auto totalLength = std::size_t {0};

            for (auto& entry: history)
                totalLength += frecencysearch::normaliseQuery(entry.title).size()
                               + frecencysearch::normaliseQuery(entry.url).size();

            check(totalLength > 0);
        });
}

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

double measureRankingMs(const std::vector<HistoryEntry>& history, const std::string& query)
{
    auto usages = std::unordered_map<std::string, double> {};
    auto lookUpUsage = [&usages](const std::string& url)
    {
        auto found = usages.find(url);
        return found != usages.end() ? found->second : 0.0;
    };

    return measureFastestMs([&] { rankPalette({}, history, {query, PaletteMode::goFromHere, lookUpUsage}); });
}
} // namespace

auto tRankingFullHistoryFitsBudget = test("PaletteSpeed/ranking a full history fits a keystroke's budget") = []
{
    auto history = makeFullHistory();

    auto baselineMs = measureBaselinePassMs(history);

    for (auto query: {"e", "ex", "exa", "page", "news"})
    {
        auto rankingInBaselinePasses = measureRankingMs(history, query) / baselineMs;
        check(rankingInBaselinePasses < rankingBudgetInBaselinePasses);
    }
};
