#include <Palette.h>

#include <NanoTest/NanoTest.h>

#include <map>

using namespace nano;
using namespace grazer;

namespace
{
std::vector<GoItem> someTabs()
{
    return {{0, "GitHub", "https://github.com"},
            {1, "Hacker News", "https://news.ycombinator.com"}};
}

std::vector<Place> somePlaces()
{
    return {{"GitHub", "https://github.com"}, {"Linear", "https://linear.app"}};
}

LastUsedFn lastUsedFrom(std::map<std::string, std::int64_t> stamps)
{
    return [stamps = std::move(stamps)](const std::string& url)
    {
        auto it = stamps.find(url);
        return it != stamps.end() ? it->second : 0;
    };
}
} // namespace

auto tOpenPlacesAreNotDuplicated =
    test("Palette/a place that's already an open tab appears once") = []
{
    auto items = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));

    auto githubRows = 0;

    for (auto& item: items)
        if (item.url.find("github.com") != std::string::npos)
            githubRows++;

    check(githubRows == 1);
    check(items.size() == 3); // two tabs + linear
};

auto tTabsInheritBookmarkFlag =
    test("Palette/an open tab of a saved place shows as bookmarked") = []
{
    auto items = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));

    for (auto& item: items)
    {
        if (item.tabId == 0)
            check(item.bookmarked);

        if (item.tabId == 1)
            check(!item.bookmarked);
    }
};

auto tMruOrdersBrowseList = test("Palette/most recently used comes first") = []
{
    auto items = mergePaletteItems(
        someTabs(),
        somePlaces(),
        lastUsedFrom({{"linear.app", 300}, {"news.ycombinator.com", 200}}));

    check(items[0].url == "https://linear.app");
    check(items[1].url == "https://news.ycombinator.com");
};

auto tEmptyQueryKeepsBrowseList =
    test("Palette/an empty query returns the browse list untouched") = []
{
    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, {}, "");

    check(ranked.size() == all.size());
};

auto tQueryFiltersAndAppendsSearch =
    test("Palette/a query filters and pins the search row last") = []
{
    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, {}, "github");

    check(ranked.size() == 2);
    check(ranked.front().url == "https://github.com");
    check(ranked.back().isSearch);
};

auto tNoMatchLeavesOnlySearch =
    test("Palette/nothing matching leaves just the search row") = []
{
    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, {}, "zzzzqqqq");

    check(ranked.size() == 1);
    check(ranked.front().isSearch);
};

auto tHistoryJoinsTheHunt = test("Palette/history rows join query results") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://webkit.org/blog", "WebKit Blog", 100, 3}};

    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, history, "webkit");

    check(ranked.size() == 2);
    check(ranked.front().url == "https://webkit.org/blog");
    check(ranked.front().tabId == -1);
};

auto tHistoryNeverDuplicatesOpenRows =
    test("Palette/history of an open tab or place is skipped") = []
{
    auto history =
        std::vector<HistoryEntry> {{"https://github.com/", "GitHub", 100, 5}};

    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, history, "github");

    check(ranked.size() == 2); // the open tab + the search row
    check(ranked.front().tabId == 0);
};

auto tSearchRowStyle = test("Palette/the search row mirrors the query kind") = []
{
    check(searchRow("github.com").title == "Open github.com");
    check(searchRow("hello world").title.find("Search Google") != std::string::npos);
};

auto tURLQueryWithoutExactMatchOpensTheURL =
    test("Palette/a URL query beats fuzzy hits when nothing matches exactly") = []
{
    auto all =
        std::vector<GoItem> {{0, "Inbox - Tamber Mail", "https://mail.google.com/"}};
    auto ranked = rankPalette(all, {}, "google.com");

    check(ranked.front().isSearch);
    check(ranked.front().url == "https://google.com");
    check(ranked.back().url == "https://mail.google.com/");
};

auto tDeadPagesSinkBelowSearch =
    test("Palette/a dead page never outranks the search row") = []
{
    auto deadTab = GoItem {};
    deadTab.tabId = 0;
    deadTab.title = "Can't reach this page";
    deadTab.url = "https://hackernews.com/";
    deadTab.failed = true;

    auto ranked = rankPalette({deadTab}, {}, "hacker news");

    check(ranked.size() == 2);
    check(ranked.front().isSearch);
    check(ranked.back().failed);
};

auto tURLQueryPrefersTheExactItem =
    test("Palette/a URL query's exact match replaces the search row") = []
{
    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, {}, "github.com");

    check(ranked.front().tabId == 0);

    for (auto& item: ranked)
        check(!item.isSearch);
};

auto tCompletionTakesTheRowsURL =
    test("Palette/Tab takes a row's URL, or the search row's query") = []
{
    check(completionText({0, "GitHub", "https://github.com"}, "git")
          == "https://github.com");
    check(completionText(searchRow("hello world"), "hello world") == "hello world");
};

auto tEditedURLOpensWhatsTyped =
    test("Palette/an edited row URL opens as typed, not the old row") = []
{
    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, {}, "https://github.com/jamierpond/Grazer");

    check(ranked.front().isSearch);
    check(ranked.front().url == "https://github.com/jamierpond/Grazer");
};

auto tSearchRowNeverDuplicated =
    test("Palette/a past search of the same query yields one row, not two") = []
{
    auto history =
        std::vector<HistoryEntry> {{"https://www.google.com/search?q=hacker+news",
                                    "hacker news - Google Search",
                                    100,
                                    1}};

    auto ranked = rankPalette({}, history, "hacker news");

    check(ranked.size() == 1);
    check(ranked.front().isSearch);
};

namespace
{
std::vector<HistoryEntry> someGoogleSignInHistory()
{
    return {{"https://accounts.google.com/ServiceLogin?access_type=offline&app_domain="
             "https%3A%2F%2Flocalhost%3A3000&client_id=12345.apps.googleusercontent.com"
             "&response_type=code&scope=email%20profile&state=abc",
             "Sign in - Google Accounts",
             100,
             1},
            {"https://accounts.google.com/v3/signin/identifier?continue=https%3A%2F%2F"
             "mail.google.com%2Fmail%2Fu%2F0%2F&followup=https%3A%2F%2Fmail.google.com"
             "&passive=1209600&service=mail&flowName=GlifWebSignIn",
             "Sign in - Google Accounts",
             90,
             1},
            {"https://accounts.google.com/o/oauth2/auth?redirect_uri=https%3A%2F%2F"
             "developers.google.com%2Foauthplayground&prompt=consent&include_granted_scopes"
             "=true&login_hint=user%40example.com",
             "Sign in - Google Accounts",
             80,
             1}};
}
} // namespace

auto tScatteredLettersAreNoHit =
    test("Palette/a query only scattered through long URLs shows just the search row") = []
{
    auto ranked =
        rankPalette({}, someGoogleSignInHistory(), "leopard eating faces party");

    check(ranked.size() == 1);
    check(ranked.front().isSearch);
};

auto tRealQueryStillHitsLongURLs =
    test("Palette/a real query still lists rows with long URLs above search") = []
{
    auto ranked = rankPalette({}, someGoogleSignInHistory(), "sign in google");

    check(ranked.size() == 4);
    check(!ranked.front().isSearch);
    check(ranked.back().isSearch);
};

auto tShortQueryHitsShortTitle =
    test("Palette/a short query still hits a short title") = []
{
    auto ranked = rankPalette(someTabs(), {}, "git");

    check(ranked.front().title == "GitHub");
};

namespace
{
std::vector<HistoryEntry> someClaudeHistory(int count)
{
    auto history = std::vector<HistoryEntry> {};

    for (auto i = 0; i < count; ++i)
        history.push_back({"https://claude.ai/chat/" + std::to_string(i),
                           "Sign in to Claude · Chat " + std::to_string(i),
                           i,
                           1});

    return history;
}
} // namespace

auto tGmailHistoryHits = test("Palette/gmail finds both Gmail inboxes, the one titled Gmail first") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://mail.google.com/mail/u/0/#inbox",
         "Inbox (12) - someone@example.com - Example Mail",
         100,
         4},
        {"https://mail.google.com/mail/u/1/#inbox", "Inbox - Gmail", 90, 2}};

    auto ranked = rankPalette({}, history, "gmail");

    check(ranked.size() == 3);
    check(ranked.front().url == "https://mail.google.com/mail/u/1/#inbox");
};

auto tGithubTitleHits =
    test("Palette/github hits a page titled Sign in to GitHub · GitHub") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://github.com/login", "Sign in to GitHub · GitHub", 100, 1}};

    auto ranked = rankPalette({}, history, "github");

    check(ranked.size() == 2);
    check(ranked.front().url == "https://github.com/login");
};

auto tEmptyQueryShowsRecentHistory =
    test("Palette/an empty query lists recent history after tabs and places") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://old.example.com", "Old", 100, 1},
        {"https://github.com/", "GitHub", 500, 5},
        {"https://new.example.com", "New", 300, 1}};

    auto all = mergePaletteItems(someTabs(), somePlaces(), lastUsedFrom({}));
    auto ranked = rankPalette(all, history, "");

    check(ranked.size() == all.size() + 2);
    check(ranked[all.size()].url == "https://new.example.com");
    check(ranked[all.size() + 1].url == "https://old.example.com");
};

auto tEmptyQueryHistoryIsCapped =
    test("Palette/an empty query lists at most 50 history rows") = []
{
    constexpr auto historySize = 200;
    constexpr auto maxRecentHistoryRows = 50;
    auto ranked = rankPalette({}, someClaudeHistory(historySize), "");

    check(ranked.size() == maxRecentHistoryRows);
    check(ranked.front().url == "https://claude.ai/chat/199");
};

auto tHitsAreCapped = test("Palette/a query lists at most 50 hits plus search") = []
{
    constexpr auto historySize = 200;
    constexpr auto maxHitRowsPlusSearch = 51;
    auto ranked = rankPalette({}, someClaudeHistory(historySize), "claude");

    check(ranked.size() == maxHitRowsPlusSearch);
    check(ranked.back().isSearch);
};

auto tDuplicateHistoryListedOnce =
    test("Palette/history entries for the same place are listed once") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://webkit.org/blog", "WebKit Blog", 100, 3},
        {"https://www.webkit.org/blog/", "WebKit Blog", 90, 1}};

    auto ranked = rankPalette({}, history, "webkit");

    check(ranked.size() == 2);
};
