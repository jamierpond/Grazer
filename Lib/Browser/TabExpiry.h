#pragma once

#include "Session.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace grazer
{
// One open tab as the idle-tab check sees it: when it was brought to the
// front (oldest first), and whether it must stay open regardless (the
// active tab, a Home tab, a tab playing audio).
struct TabUse
{
    std::vector<std::int64_t> activatedAtMs;
    bool mustStayOpen = false;
};

// Adds an activation at `nowMs`, keeping only the most recent ones.
std::vector<std::int64_t> recordTabActivation(std::vector<std::int64_t> activatedAtMs,
                                              std::int64_t nowMs);

// Moves every activation later by `awayMs`, so time Grazer wasn't running
// doesn't count as idle time.
std::vector<std::int64_t> skipTimeAway(std::vector<std::int64_t> activatedAtMs,
                                       std::int64_t awayMs);

// skipTimeAway for every tab in `state`, by how long ago it was saved: idle
// time only counts while Grazer runs.
SessionState skipTimeGrazerWasClosed(SessionState state, std::int64_t nowMs);

// How long the user usually takes to come back to a tab: the 90th percentile
// of the gaps between consecutive activations across all tabs, clamped to a
// sane range. A default stands in until enough gaps have been seen.
std::int64_t calculateRevisitGapCutoffMs(const std::vector<TabUse>& tabs);

// How long `tab` may sit unused before it closes: the cutoff, times how
// often the tab has been used (capped).
std::int64_t calculateAllowedIdleMs(const TabUse& tab, std::int64_t revisitGapCutoffMs);

// Indexes into `tabs` of those idle longer than they're allowed to be.
std::vector<std::size_t> findIdleTabsToClose(const std::vector<TabUse>& tabs,
                                             std::int64_t nowMs);
} // namespace grazer
