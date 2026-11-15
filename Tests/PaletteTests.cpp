#include <Palette.h>

#include <NanoTest/NanoTest.h>

#include <map>

using namespace nano;
using namespace grazer;

namespace
{
constexpr auto goFromHere = PaletteMode::goFromHere;
constexpr auto newTab = PaletteMode::newTab;

PaletteTab makeTab(std::int64_t id,
                   const std::string& title,
                   const std::string& url,
                   std::int64_t lastViewedAtMs = 0)
{
    auto tab = PaletteTab {};
    tab.tabId = id;
    tab.title = title;
    tab.url = url;
    tab.lastViewedAtMs = lastViewedAtMs;
    return tab;
}

std::vector<PaletteTab> someTabs()
{
    return {makeTab(0, "GitHub", "https://github.com", 100),
            makeTab(1, "Hacker News", "https://news.ycombinator.com", 200)};
}

std::vector<Place> somePlaces()
{
    return {{"GitHub", "https://github.com"}, {"Linear", "https://linear.app"}};
}

UsageFn usageFrom(std::map<std::string, double> usage)
{
    return [usage = std::move(usage)](const std::string& url)
    {
        auto it = usage.find(url);
        return it != usage.end() ? it->second : 0.0;
    };
}

std::vector<GoItem> someRows()
{
    return listPaletteRows(someTabs(), somePlaces(), usageFrom({}));
}

std::vector<GoItem> rank(const std::vector<GoItem>& rows,
                         const std::vector<HistoryEntry>& history,
                         const std::string& query)
{
    return rankPalette(rows, history, {query, goFromHere, usageFrom({})});
}

GoItem makeTabRow(std::int64_t id, const std::string& title, const std::string& url)
{
    return listPaletteRows({makeTab(id, title, url)}, {}, usageFrom({})).front();
}
} // namespace

auto tOpenPlacesAreNotDuplicated =
    test("Palette/a starred place that's also an open tab shows once, as the tab with a star") = []
{
    auto rows = someRows();

    auto githubRows = 0;

    for (auto& row: rows)
    {
        if (row.url.find("github.com") == std::string::npos)
            continue;

        githubRows++;
        check(row.kind == GoKind::tab);
        check(row.bookmarked);
    }

    check(githubRows == 1);
    check(rows.size() == 3);
};

auto tTabsByLastViewedThenPlaces =
    test("Palette/no text lists tabs by last viewed, then starred places not open") = []
{
    auto rows = listPaletteRows(
        someTabs(), somePlaces(), usageFrom({{"linear.app", 999}}));

    check(rows[0].url == "https://news.ycombinator.com");
    check(rows[1].url == "https://github.com");
    check(rows[2].url == "https://linear.app");
    check(rows[2].kind == GoKind::place);
};

auto tCurrentTabListedLast =
    test("Palette/the current tab is listed last and is never the Enter default") = []
{
    auto tabs = someTabs();
    tabs[1].isCurrent = true;

    auto rows = listPaletteRows(tabs, somePlaces(), usageFrom({}));

    check(rows.back().tabId == 1);
    check(rows.back().isCurrent);
    check(findDefaultSelection(rows) == 0);
    check(!rows[(std::size_t) findDefaultSelection(rows)].isCurrent);
};

auto tOnlyCurrentTabHasNoSelection =
    test("Palette/with only the current tab listed nothing is highlighted") = []
{
    auto tab = makeTab(0, "GitHub", "https://github.com");
    tab.isCurrent = true;

    check(findDefaultSelection(listPaletteRows({tab}, {}, usageFrom({}))) == -1);
    check(findDefaultSelection({}) == -1);
};

auto tCurrentTabNeverFirstWhenTyping =
    test("Palette/typing the current tab's name still lists it last") = []
{
    auto tabs = someTabs();
    tabs[0].isCurrent = true;
    auto ranked = rank(listPaletteRows(tabs, {}, usageFrom({})), {}, "github");

    check(ranked.front().kind == GoKind::search);
    check(ranked.back().isCurrent);
};

auto tCurrentURLOpensInThisTab =
    test("Palette/the current URL as text puts Open <url> in this tab first") = []
{
    auto tabs = someTabs();
    tabs[0].isCurrent = true;
    auto ranked = rank(listPaletteRows(tabs, {}, usageFrom({})), {}, "https://github.com");

    check(ranked.front().kind == GoKind::typedURL);
    check(ranked.front().title == "Open https://github.com in this tab");
    check(ranked.back().isCurrent);
};

auto tTypedURLTitleFollowsMode =
    test("Palette/new-tab mode titles the typed URL row Open <url>") = []
{
    auto ranked = rankPalette({}, {}, {"example.com", newTab, usageFrom({})});

    check(ranked.front().title == "Open example.com");
};

auto tEmptyQueryKeepsTheRows =
    test("Palette/an empty query returns the rows untouched") = []
{
    auto rows = someRows();
    check(rank(rows, {}, "").size() == rows.size());
};

auto tStrongHitFirstSearchSecond =
    test("Palette/a strong hit comes first with search second") = []
{
    auto ranked = rank(someRows(), {}, "github");

    check(ranked.size() == 2);
    check(ranked[0].url == "https://github.com");
    check(ranked[1].kind == GoKind::search);
};

auto tWeakHitPutsSearchFirst =
    test("Palette/a weak hit puts search first") = []
{
    auto ranked = rank({makeTabRow(0, "Linear issues", "https://linear.app")}, {}, "lnir");

    check(ranked.front().kind == GoKind::search);
};

auto tNoMatchLeavesOnlySearch =
    test("Palette/nothing matching leaves just the search row") = []
{
    auto ranked = rank(someRows(), {}, "zzzzqqqq");

    check(ranked.size() == 1);
    check(ranked.front().kind == GoKind::search);
};

auto tHistoryJoinsTheHunt = test("Palette/history rows join query results") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://webkit.org/blog", "WebKit Blog", 100, 3}};

    auto ranked = rank(someRows(), history, "webkit");

    check(ranked.size() == 2);
    check(ranked.front().url == "https://webkit.org/blog");
    check(ranked.front().kind == GoKind::history);
};

auto tHistoryNeverDuplicatesOpenRows =
    test("Palette/history of an open tab or place is skipped") = []
{
    auto history =
        std::vector<HistoryEntry> {{"https://github.com/", "GitHub", 100, 5}};

    auto ranked = rank(someRows(), history, "github");

    check(ranked.size() == 2);
    check(ranked.front().tabId == 0);
};

auto tSearchRowTitle = test("Palette/the search row says what it searches for") = []
{
    auto ranked = rank({}, {}, "hello world");

    check(ranked.front().title == "Search Google for “hello world”");
};

auto tURLQueryWithoutExactMatchOpensTheURL =
    test("Palette/a URL query beats fuzzy hits when nothing matches exactly") = []
{
    auto ranked =
        rank({makeTabRow(0, "Inbox - Tamber Mail", "https://mail.google.com/")}, {}, "google.com");

    check(ranked.front().kind == GoKind::typedURL);
    check(ranked.front().url == "https://google.com");
};

auto tDeadPagesSinkBelowSearch =
    test("Palette/a dead page never outranks the search row") = []
{
    auto deadTab = makeTab(0, "Can't reach this page", "https://hackernews.com/");
    deadTab.failed = true;

    auto ranked = rank(listPaletteRows({deadTab}, {}, usageFrom({})), {}, "hackernews");

    check(ranked.size() == 2);
    check(ranked.front().kind == GoKind::search);
    check(ranked.back().failed);
};

auto tURLQueryPrefersTheExactItem =
    test("Palette/a URL query's exact match replaces the Open row") = []
{
    auto ranked = rank(someRows(), {}, "github.com");

    check(ranked.front().tabId == 0);

    for (auto& row: ranked)
        check(row.kind != GoKind::typedURL && row.kind != GoKind::search);
};

auto tCompletionTakesTheRowsURL =
    test("Palette/Tab takes a row's URL, or the search row's query") = []
{
    check(completionText(makeTabRow(0, "GitHub", "https://github.com"), "git")
          == "https://github.com");
    check(completionText(rank({}, {}, "hello world").front(), "hello world")
          == "hello world");
};

auto tEditedURLOpensWhatsTyped =
    test("Palette/an edited row URL opens as typed, not the old row") = []
{
    auto ranked = rank(someRows(), {}, "https://github.com/jamierpond/Grazer");

    check(ranked.front().kind == GoKind::typedURL);
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

    auto ranked = rank({}, history, "hacker news");

    check(ranked.size() == 1);
    check(ranked.front().kind == GoKind::search);
};

auto tRowKinds = test("Palette/each row says what it is and what ✕ and ★ do") = []
{
    auto history = std::vector<HistoryEntry> {{"https://webkit.org", "WebKit", 1, 1}};
    auto ranked = rank(someRows(), history, "i");

    for (auto& row: ranked)
    {
        if (row.kind == GoKind::tab)
            check(row.canClose && row.canStar);

        if (row.kind == GoKind::place)
            check(!row.canClose && row.canStar && row.bookmarked);

        if (row.kind == GoKind::history)
            check(row.canClose && row.canStar);

        if (row.kind == GoKind::search || row.kind == GoKind::typedURL)
            check(!row.canClose && !row.canStar);
    }
};

auto tOpeningFollowsTheMode =
    test("Palette/a tab row switches, other rows load here or in a new tab by mode and ⌘") = []
{
    auto tab = makeTabRow(0, "GitHub", "https://github.com");
    auto place = someRows()[2];

    check(decidePaletteOpening(tab, goFromHere, false) == PaletteOpening::switchToTab);
    check(decidePaletteOpening(tab, newTab, true) == PaletteOpening::switchToTab);
    check(decidePaletteOpening(place, goFromHere, false) == PaletteOpening::loadInThisTab);
    check(decidePaletteOpening(place, goFromHere, true) == PaletteOpening::openInNewTab);
    check(decidePaletteOpening(place, newTab, false) == PaletteOpening::openInNewTab);
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
        rank({}, someGoogleSignInHistory(), "leopard eating faces party");

    check(ranked.size() == 1);
    check(ranked.front().kind == GoKind::search);
};

auto tRealQueryStillHitsLongURLs =
    test("Palette/a real query still lists rows with long URLs") = []
{
    auto ranked = rank({}, someGoogleSignInHistory(), "sign in google");

    check(ranked.size() == 4);
    check(ranked.front().kind == GoKind::history);
};

auto tShortQueryHitsShortTitle =
    test("Palette/a short query still hits a short title") = []
{
    auto ranked = rank(someRows(), {}, "git");

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

auto tGmailHistoryHits = test("Palette/gmail finds the inbox titled Gmail, not one only at mail.google.com") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://mail.google.com/mail/u/0/#inbox",
         "Inbox (12) - someone@example.com - Example Mail",
         100,
         4},
        {"https://mail.google.com/mail/u/1/#inbox", "Inbox - Gmail", 90, 2}};

    auto ranked = rank({}, history, "gmail");

    check(ranked.size() == 2);
    check(ranked.front().url == "https://mail.google.com/mail/u/1/#inbox");
};

auto tGithubTitleHits =
    test("Palette/github hits a page titled Sign in to GitHub · GitHub") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://github.com/login", "Sign in to GitHub · GitHub", 100, 1}};

    auto ranked = rank({}, history, "github");

    check(ranked.size() == 2);
    check(ranked[0].url == "https://github.com/login");
    check(ranked[1].kind == GoKind::search);
};

auto tEmptyQueryShowsNoHistory =
    test("Palette/an empty query lists only tabs and places, no history") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://old.example.com", "Old", 100, 1},
        {"https://new.example.com", "New", 300, 1}};

    auto all = someRows();
    auto ranked = rank(all, history, "");

    check(ranked.size() == all.size());

    for (auto& item: ranked)
        check(item.url != "https://old.example.com" && item.url != "https://new.example.com");
};

auto tHitsAreCapped = test("Palette/a query lists at most 50 hits plus search") = []
{
    constexpr auto historySize = 200;
    constexpr auto maxHitRowsPlusSearch = 51;
    auto ranked = rank({}, someClaudeHistory(historySize), "claude");

    check(ranked.size() == maxHitRowsPlusSearch);
};

auto tDuplicateHistoryListedOnce =
    test("Palette/history entries for the same place are listed once") = []
{
    auto history = std::vector<HistoryEntry> {
        {"https://webkit.org/blog", "WebKit Blog", 100, 3},
        {"https://www.webkit.org/blog/", "WebKit Blog", 90, 1}};

    auto ranked = rank({}, history, "webkit");

    check(ranked.size() == 2);
};

auto tOpenPageHidesItsFragmentHistory =
    test("Palette/an open tab hides history rows for the same page at another #fragment") = []
{
    auto tabs = std::vector<GoItem> {makeTabRow(0, "PR files", "https://github.com/o/r/pull/1/files")};
    auto history = std::vector<HistoryEntry> {
        {"https://github.com/o/r/pull/1/files#diff-aaa", "PR files", 1000, 1}};

    auto ranked = rank(tabs, history, "pr files");

    check(ranked.size() == 2);
    check(ranked.front().kind == GoKind::tab);
};

auto tHighlightMovesToTheNextRow =
    test("Palette/closing the highlighted row highlights the next one") = []
{
    check(keepSelectionInRange(1, 3) == 1);
    check(keepSelectionInRange(2, 2) == 1);
    check(keepSelectionInRange(-1, 3) == -1);
    check(keepSelectionInRange(0, 0) == -1);
};
