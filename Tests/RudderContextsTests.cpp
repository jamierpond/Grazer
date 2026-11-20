#include "TestSupport.h"

#include <RudderContexts.h>

#include <GrazerPalette/Url.h>
#include <NanoTest/NanoTest.h>

#include <filesystem>

using namespace nano;
using namespace grazer;

namespace
{
constexpr std::int64_t usedAtMs = 1'790'000'000'000LL;
constexpr std::int64_t visitedAtMs = usedAtMs - 1000;
} // namespace

auto tOneContextPerTab = test("RudderContexts/one context per tab, titled by its url when it has no title") = []
{
    auto contexts = buildRudderContexts({{3, "Pull requests", "https://github.com/pulls", true},
                                         {4, "", "https://example.com/new", false}});

    check(contexts.size() == 2);
    check(contexts[0].id == "3" && contexts[0].focused && contexts[0].open);
    check(contexts[1].title == "https://example.com/new" && !contexts[1].focused);
};

auto tMaterialsFromStores = test("RudderContexts/the ranking materials carry the tabs and every stamp, place and history entry") = []
{
    auto db = testing::freshDatabase("rudder-materials-stores");
    auto mru = MruStore {db};
    auto history = HistoryStore {db};
    auto places = PlacesStore {db, std::filesystem::temp_directory_path() / "GrazerTests" / "no-such-legacy.json"};

    mru.touch("github.com/pulls", usedAtMs);
    history.recordVisit("https://github.com/pulls", "Pull requests", visitedAtMs);

    auto tab = PaletteTab {};
    tab.tabId = 3;
    tab.url = "https://github.com/pulls";
    auto pushed = buildRudderContexts({{3, "Pull requests", "https://github.com/pulls", true}});
    auto materials = collectPaletteRankingMaterials({tab}, pushed, {mru, history, places});

    check(materials.tabs.size() == 1 && materials.tabs.front().tabId == 3);
    check(materials.tabs.front().isCurrent, "the focused tab is the current one");
    check(materials.stampsByNormalizedUrl.at("github.com/pulls").at == usedAtMs);
    check(materials.history.size() == 1 && materials.history.front().visits == 1);
    check(materials.places.size() == places.all().size());
};

auto tTabIdsAreNotPages = test("RudderContexts/a tab's id is not a page to open") = []
{
    check(parseRudderPageContextId("url:https://github.com/pulls")
          == std::optional<std::string> {"https://github.com/pulls"});
    check(!parseRudderPageContextId("3"));
    check(!parseRudderPageContextId("url:"));
    check(parseRudderTabId("3") == std::optional<std::int64_t> {3});
};

auto tMaterialsAnswer = test("RudderContexts/the answer names Grazer's palette ordering") = []
{
    auto db = testing::freshDatabase("rudder-materials-answer");
    auto mru = MruStore {db};
    auto history = HistoryStore {db};
    auto places = PlacesStore {db, std::filesystem::temp_directory_path() / "GrazerTests" / "no-such-legacy.json"};
    auto answer = collectRudderRankingMaterials({}, {}, {mru, history, places});

    check(answer.ordering == paletteRankingMaterialsName && answer.materials.isObject());
};
