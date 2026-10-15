#include "TabExpiry.h"

namespace grazer
{
std::vector<std::int64_t> expiredTabs(std::int64_t nowMs,
                                      const std::vector<TabVisit>& tabs,
                                      const std::set<std::int64_t>& activeIds)
{
    auto expired = std::vector<std::int64_t> {};

    for (auto& tab: tabs)
        if (!activeIds.contains(tab.id) && nowMs - tab.lastVisitedMs >= tabExpiryMs)
            expired.push_back(tab.id);

    return expired;
}
} // namespace grazer
