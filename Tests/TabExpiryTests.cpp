#include "TabExpiry.h"

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

namespace
{
constexpr std::int64_t now = 100LL * 60 * 60 * 1000;
constexpr std::int64_t minute = 60 * 1000;
} // namespace

auto tOldTabsExpire = test("TabExpiry/a tab unvisited for the expiry time goes") = []
{
    auto tabs = std::vector<TabVisit> {{1, now - tabExpiryMs},
                                       {2, now - tabExpiryMs + minute},
                                       {3, now - 3 * tabExpiryMs},
                                       {4, now}};

    check(expiredTabs(now, tabs, {}) == std::vector<std::int64_t> {1, 3});
};

auto tActiveTabsStay = test("TabExpiry/a window's active tab never expires") = []
{
    auto tabs = std::vector<TabVisit> {{1, 0}, {2, 0}, {3, 0}};

    check(expiredTabs(now, tabs, {1, 3}) == std::vector<std::int64_t> {2});
};

auto tNothingToDo = test("TabExpiry/no tabs, nothing expires") = []
{
    check(expiredTabs(now, {}, {}).empty());
};
