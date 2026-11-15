#include <TabExpiry.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

namespace
{
constexpr std::int64_t hourMs = 60 * 60 * 1000;
constexpr std::int64_t dayMs = 24 * hourMs;
constexpr std::int64_t now = 100 * dayMs;

TabUse makeTabUsedAt(std::vector<std::int64_t> activatedAtMs)
{
    return {std::move(activatedAtMs), false};
}

bool isClosed(const std::vector<std::size_t>& closed, std::size_t index)
{
    return std::find(closed.begin(), closed.end(), index) != closed.end();
}
} // namespace

auto tOneOffFromLongAgoCloses =
    test("TabExpiry/a tab opened once long ago closes") = []
{
    auto tabs = std::vector<TabUse> {makeTabUsedAt({now - 3 * dayMs}),
                                     makeTabUsedAt({now - hourMs})};

    auto closed = findIdleTabsToClose(tabs, now);

    check(isClosed(closed, 0));
    check(!isClosed(closed, 1));
};

auto tFrequentTabSurvivesGapThatClosesOneOff =
    test("TabExpiry/a frequently revisited tab survives a gap that closes a one-off") = []
{
    auto lastUse = now - 2 * dayMs;
    auto frequent = makeTabUsedAt(
        {lastUse - 3 * hourMs, lastUse - 2 * hourMs, lastUse - hourMs, lastUse});
    auto oneOff = makeTabUsedAt({lastUse});

    auto closed = findIdleTabsToClose({frequent, oneOff}, now);

    check(!isClosed(closed, 0));
    check(isClosed(closed, 1));
};

auto tProtectedTabsNeverClose =
    test("TabExpiry/the active, Home and audio-playing tabs never close") = []
{
    auto stale = TabUse {{now - 365 * dayMs}, true};

    check(findIdleTabsToClose({stale}, now).empty());
};

auto tCutoffFollowsObservedGaps =
    test("TabExpiry/the cutoff is the 90th percentile of revisit gaps") = []
{
    auto times = std::vector<std::int64_t> {};

    for (std::int64_t i = 0; i <= 10; ++i)
        times.push_back(i * 5 * hourMs);

    check(calculateRevisitGapCutoffMs({makeTabUsedAt(times)}) == 5 * hourMs);
};

auto tTooFewGapsUsesDefault =
    test("TabExpiry/with too few revisits the cutoff is one day") = []
{
    check(calculateRevisitGapCutoffMs({makeTabUsedAt({0, hourMs})}) == dayMs);
};

auto tTimeAwayDoesNotCount =
    test("TabExpiry/time Grazer wasn't running is skipped") = []
{
    auto shifted = skipTimeAway({hourMs, 2 * hourMs}, dayMs);

    check(shifted == std::vector<std::int64_t> {dayMs + hourMs, dayMs + 2 * hourMs});
};

auto tActivationsAreCapped =
    test("TabExpiry/only the most recent activations are kept") = []
{
    auto times = std::vector<std::int64_t> {};

    for (std::int64_t i = 0; i < 100; ++i)
        times = recordTabActivation(std::move(times), i);

    check(times.size() < 100);
    check(times.back() == 99);
};

auto tSessionSkipsTimeClosed =
    test("TabExpiry/a restored session's activations move past the time Grazer was closed") = []
{
    auto window = WindowSession {{"https://a.com"}, 0, {{{hourMs}}}};
    auto state = skipTimeGrazerWasClosed({{window}, 2 * hourMs}, 5 * hourMs);

    check(state.windows[0].activations[0].atMs == std::vector<std::int64_t> {4 * hourMs});
};
