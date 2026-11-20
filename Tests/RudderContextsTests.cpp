#include "TestSupport.h"

#include <RudderContexts.h>
#include <Url.h>

#include <FrecencySearch/LearnedPicks.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

namespace
{
constexpr std::int64_t usedAtMs = 1'790'000'000'000LL;
constexpr std::int64_t visitedAtMs = usedAtMs - 1000;
constexpr double decayedUses = 2.5;
constexpr std::int64_t lifetimeVisits = 12;

RecordedPageUse makeRecordedUseOfGitHub()
{
    auto recorded = RecordedPageUse {};
    recorded.stampsByNormalizedUrl["github.com/pulls"] = {usedAtMs, decayedUses};
    recorded.historyByPageKey["github.com/pulls"] = {
        "https://github.com/pulls", "Pull requests", visitedAtMs, lifetimeVisits};
    return recorded;
}
} // namespace

auto tTabCarriesUsage = test("RudderContexts/a tab carries the use Grazer recorded for its page") = []
{
    auto contexts = buildRudderContexts({{3, "Pull requests", "https://github.com/pulls", true}},
                                        makeRecordedUseOfGitHub());

    check(contexts.size() == 1);

    auto& tab = contexts.front();
    check(tab.id == "3" && tab.open && tab.focused);
    check(tab.usage && tab.usage->lastUsedAtMs == usedAtMs);
    check(tab.usage && tab.usage->useCount
          && *tab.usage->useCount
                 == rudder::protocol::DecayingUseCount {
                     decayedUses, usedAtMs, frecencysearch::usageHalfLifeMs});
    check(tab.usage && tab.usage->lastVisitAtMs == visitedAtMs
          && tab.usage->visitCount == lifetimeVisits);
    check(tab.usage && !tab.usage->lastTypedAtMs && !tab.usage->lastFileChangeAtMs);
};

auto tTabWithoutUsage = test("RudderContexts/a tab Grazer has no record of reports no usage") = []
{
    auto contexts = buildRudderContexts({{4, "", "https://example.com/new", false}}, {});

    check(contexts.size() == 1);
    check(contexts.front().title == "https://example.com/new");
    check(!contexts.front().usage);
};

auto tUnopenedPage = test("RudderContexts/a used page with no tab is an unopened context with its usage") = []
{
    auto contexts = buildRudderContexts({}, makeRecordedUseOfGitHub());

    check(contexts.size() == 1);

    auto& page = contexts.front();
    check(!page.open);
    check(page.title == "Pull requests");
    check(page.url == "https://github.com/pulls");
    check(parseRudderPageContextId(page.id) == std::optional<std::string> {"https://github.com/pulls"});
    check(page.usage && page.usage->lastUsedAtMs == usedAtMs);
};

auto tTabCoversPage = test("RudderContexts/a page open in a tab isn't listed again") = []
{
    auto contexts = buildRudderContexts({{3, "PRs", "https://www.github.com/pulls/", false}},
                                        makeRecordedUseOfGitHub());

    check(contexts.size() == 1 && contexts.front().open);
};

auto tPageWithoutHistory = test("RudderContexts/a used page missing from history opens from its key") = []
{
    auto recorded = RecordedPageUse {};
    recorded.stampsByNormalizedUrl["news.ycombinator.com"] = {usedAtMs, decayedUses};
    auto contexts = buildRudderContexts({}, recorded);

    check(contexts.size() == 1);
    check(contexts.front().title == "news.ycombinator.com");
    check(contexts.front().url == navigationURL("news.ycombinator.com"));
    check(contexts.front().usage && !contexts.front().usage->visitCount);
};

auto tTabIdsAreNotPages = test("RudderContexts/a tab's id is not a page to open") = []
{
    check(!parseRudderPageContextId("3"));
    check(!parseRudderPageContextId("url:"));
};

auto tReadsStores = test("RudderContexts/reads every stamp and history entry from the stores") = []
{
    auto db = testing::freshDatabase("rudder-contexts-stores");
    auto mru = MruStore {db};
    auto history = HistoryStore {db};

    mru.touch("github.com/pulls", usedAtMs);
    history.recordVisit("https://github.com/pulls", "Pull requests", visitedAtMs);

    auto recorded = readRecordedPageUse(mru, history);

    check(recorded.stampsByNormalizedUrl.contains("github.com/pulls"));
    check(recorded.historyByPageKey.contains(pageKey("https://github.com/pulls")));
};
