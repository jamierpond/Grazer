#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace grazer
{
// A tab nobody has looked at for tabExpiryMs is closed. Grazer checks on
// every navigation, every tabSweepIntervalMs, and once on the restored
// session at launch.
constexpr std::int64_t tabExpiryMs = 2LL * 60 * 60 * 1000;
constexpr std::int64_t tabSweepIntervalMs = 5LL * 60 * 1000;

struct TabVisit
{
    std::int64_t id = -1;

    // When the user last left the tab (switched or navigated away), or when
    // it opened; a background tab still in use (a call, audio) keeps
    // pushing it forward.
    std::int64_t lastVisitedMs = 0;
};

// The ids of the tabs to close at `nowMs`: unvisited for tabExpiryMs or more,
// and not the active tab of any window (`activeIds`).
std::vector<std::int64_t> expiredTabs(std::int64_t nowMs,
                                      const std::vector<TabVisit>& tabs,
                                      const std::set<std::int64_t>& activeIds);
} // namespace grazer
