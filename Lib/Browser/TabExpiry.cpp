#include "TabExpiry.h"

#include <algorithm>

namespace grazer
{
namespace
{
constexpr std::int64_t minuteMs = 60 * 1000;
constexpr std::int64_t hourMs = 60 * minuteMs;
constexpr std::int64_t dayMs = 24 * hourMs;

constexpr std::size_t maxActivationsKept = 16;
constexpr std::size_t minGapsForEstimate = 8;
constexpr double revisitPercentile = 0.9;
constexpr std::int64_t shortestCountedGapMs = minuteMs;
constexpr std::int64_t defaultRevisitGapCutoffMs = dayMs;
constexpr std::int64_t minRevisitGapCutoffMs = 2 * hourMs;
constexpr std::int64_t maxRevisitGapCutoffMs = 3 * dayMs;
constexpr std::int64_t maxUseMultiplier = 4;

std::vector<std::int64_t> collectRevisitGaps(const std::vector<TabUse>& tabs)
{
    auto gaps = std::vector<std::int64_t> {};

    for (auto& tab: tabs)
    {
        for (std::size_t i = 1; i < tab.activatedAtMs.size(); ++i)
        {
            auto gap = tab.activatedAtMs[i] - tab.activatedAtMs[i - 1];

            if (gap >= shortestCountedGapMs)
                gaps.push_back(gap);
        }
    }

    return gaps;
}

// The value below which `fraction` of `values` fall (nearest rank).
std::int64_t findPercentile(std::vector<std::int64_t> values, double fraction)
{
    std::sort(values.begin(), values.end());
    auto lastIndex = (double) (values.size() - 1);
    auto rank = (std::size_t) (fraction * lastIndex);
    return values[rank];
}

std::int64_t getLastActivation(const TabUse& tab)
{
    if (tab.activatedAtMs.empty())
        return 0;

    return tab.activatedAtMs.back();
}
} // namespace

std::vector<std::int64_t> recordTabActivation(std::vector<std::int64_t> activatedAtMs,
                                              std::int64_t nowMs)
{
    activatedAtMs.push_back(nowMs);

    if (activatedAtMs.size() > maxActivationsKept)
    {
        auto excess = (std::ptrdiff_t) (activatedAtMs.size() - maxActivationsKept);
        activatedAtMs.erase(activatedAtMs.begin(), activatedAtMs.begin() + excess);
    }

    return activatedAtMs;
}

std::vector<std::int64_t> skipTimeAway(std::vector<std::int64_t> activatedAtMs,
                                       std::int64_t awayMs)
{
    if (awayMs <= 0)
        return activatedAtMs;

    for (auto& at: activatedAtMs)
        at += awayMs;

    return activatedAtMs;
}

SessionState skipTimeGrazerWasClosed(SessionState state, std::int64_t nowMs)
{
    if (state.savedAtMs <= 0)
        return state;

    auto awayMs = nowMs - state.savedAtMs;

    for (auto& window: state.windows)
        for (auto& tab: window.activations)
            tab.atMs = skipTimeAway(std::move(tab.atMs), awayMs);

    return state;
}

std::int64_t calculateRevisitGapCutoffMs(const std::vector<TabUse>& tabs)
{
    auto gaps = collectRevisitGaps(tabs);

    if (gaps.size() < minGapsForEstimate)
        return defaultRevisitGapCutoffMs;

    return std::clamp(findPercentile(std::move(gaps), revisitPercentile),
                      minRevisitGapCutoffMs,
                      maxRevisitGapCutoffMs);
}

std::int64_t calculateAllowedIdleMs(const TabUse& tab, std::int64_t revisitGapCutoffMs)
{
    auto useCount = std::max((std::int64_t) tab.activatedAtMs.size(), (std::int64_t) 1);
    return revisitGapCutoffMs * std::min(useCount, maxUseMultiplier);
}

std::vector<std::size_t> findIdleTabsToClose(const std::vector<TabUse>& tabs,
                                             std::int64_t nowMs)
{
    auto cutoff = calculateRevisitGapCutoffMs(tabs);
    auto idleTabs = std::vector<std::size_t> {};

    for (std::size_t i = 0; i < tabs.size(); ++i)
    {
        auto& tab = tabs[i];
        auto idleMs = nowMs - getLastActivation(tab);

        if (!tab.mustStayOpen && idleMs > calculateAllowedIdleMs(tab, cutoff))
            idleTabs.push_back(i);
    }

    return idleTabs;
}
} // namespace grazer
