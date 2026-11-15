#include "Palette.h"
#include "Url.h"

#include <FrecencySearch/MatchLevel.h>
#include <FrecencySearch/RowOrder.h>

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <utility>

namespace grazer
{
namespace
{
using frecencysearch::MatchLevel;

// A match this good (the title, or a word of it, starts with the text) beats
// searching for the text.
constexpr auto worstStrongMatchLevel = MatchLevel::nameWordStartsWithQuery;

constexpr std::size_t maxHitRows = 50;

struct ScoredRow
{
    frecencysearch::RowRankingFacts facts;
    GoItem row;
};

std::unordered_set<std::string> collectPageKeys(const std::vector<GoItem>& rows)
{
    auto keys = std::unordered_set<std::string> {};

    for (auto& row: rows)
        keys.insert(pageKey(row.url));

    return keys;
}

GoItem makeTabRow(const PaletteTab& tab, bool bookmarked)
{
    auto row = GoItem {};
    row.tabId = tab.tabId;
    row.title = tab.title.empty() ? tab.url : tab.title;
    row.url = tab.url;
    row.bookmarked = bookmarked;
    row.kind = GoKind::tab;
    row.failed = tab.failed;
    row.isCurrent = tab.isCurrent;
    row.canClose = true;
    row.canStar = true;
    return row;
}

GoItem makePlaceRow(const Place& place)
{
    auto row = GoItem {};
    row.title = place.title;
    row.url = place.url;
    row.bookmarked = true;
    row.kind = GoKind::place;
    row.canStar = true;
    return row;
}

GoItem makeHistoryRow(const HistoryEntry& entry)
{
    auto row = GoItem {};
    row.title = entry.title.empty() ? entry.url : entry.title;
    row.url = entry.url;
    row.kind = GoKind::history;
    row.canClose = true;
    row.canStar = true;
    return row;
}

GoItem makeSearchRow(const std::string& query)
{
    auto row = GoItem {};
    row.title = "Search Google for “" + query + "”";
    row.url = navigationURL(query);
    row.kind = GoKind::search;
    return row;
}

GoItem makeTypedURLRow(const std::string& query, PaletteMode mode)
{
    auto row = GoItem {};
    row.title = mode == PaletteMode::goFromHere ? "Open " + query + " in this tab"
                                                : "Open " + query;
    row.url = navigationURL(query);
    row.kind = GoKind::typedURL;
    return row;
}

std::vector<PaletteTab> sortTabsByLastViewed(std::vector<PaletteTab> tabs)
{
    std::stable_sort(tabs.begin(),
                     tabs.end(),
                     [](const PaletteTab& a, const PaletteTab& b)
                     { return a.lastViewedAtMs > b.lastViewedAtMs; });
    return tabs;
}

std::vector<GoItem> listPlacesNotOpen(const std::vector<Place>& places,
                                      const std::unordered_set<std::string>& openPages,
                                      const UsageFn& usage)
{
    auto rows = std::vector<GoItem> {};
    auto listedPages = openPages;

    for (auto& place: places)
        if (listedPages.insert(pageKey(place.url)).second)
            rows.push_back(makePlaceRow(place));

    std::stable_sort(rows.begin(),
                     rows.end(),
                     [&](const GoItem& a, const GoItem& b) {
                         return usage(normalizeURL(a.url)) > usage(normalizeURL(b.url));
                     });
    return rows;
}

// Every history entry whose page isn't already a row, in the order given.
std::vector<GoItem> appendHistoryRows(std::vector<GoItem> rows,
                                      const std::vector<HistoryEntry>& history)
{
    auto listedPages = collectPageKeys(rows);

    for (auto& entry: history)
        if (listedPages.insert(pageKey(entry.url)).second)
            rows.push_back(makeHistoryRow(entry));

    return rows;
}

frecencysearch::SearchableTexts getSearchableTexts(const GoItem& row)
{
    return {{row.title}, {frecencysearch::removeUrlScheme(row.url)}};
}

// Text typed as this row's exact URL finds it best of all.
std::optional<MatchLevel> findRowMatchLevel(const std::string& query,
                                            const std::string& normalisedQuery,
                                            const GoItem& row)
{
    if (normalizeURL(row.url) == normalizeURL(query))
        return MatchLevel::nameStartsWithQuery;

    return frecencysearch::findMatchLevel(normalisedQuery, getSearchableTexts(row));
}

// Rows the text finds, in FrecencySearch order: match level, then usage, open
// tabs before the rest.
std::vector<ScoredRow> findHits(const std::vector<GoItem>& candidates,
                                const std::string& query,
                                const UsageFn& usage)
{
    auto normalisedQuery = frecencysearch::normaliseQuery(query);
    auto hits = std::vector<ScoredRow> {};

    for (auto& row: candidates)
    {
        auto level = findRowMatchLevel(query, normalisedQuery, row);

        if (!level)
            continue;

        auto facts = frecencysearch::RowRankingFacts {};
        facts.level = *level;
        facts.usage = usage(normalizeURL(row.url));
        facts.isOpen = row.kind == GoKind::tab;
        hits.push_back({facts, row});
    }

    std::stable_sort(hits.begin(),
                     hits.end(),
                     [](const ScoredRow& a, const ScoredRow& b)
                     { return frecencysearch::shouldRankBefore(a.facts, b.facts); });

    if (hits.size() > maxHitRows)
        hits.resize(maxHitRows);

    return hits;
}

bool isSinkingRow(const GoItem& row)
{
    return row.failed || row.isCurrent;
}

// Live hits sort first, then failed pages, then the current tab.
constexpr auto liveRowLevel = 0;
constexpr auto failedRowLevel = 1;
constexpr auto currentTabLevel = 2;

int getSinkLevel(const GoItem& row)
{
    if (row.isCurrent)
        return currentTabLevel;

    return row.failed ? failedRowLevel : liveRowLevel;
}

// Hits in score order, failed pages and the current tab moved to the end.
std::vector<GoItem> orderHitsForDisplay(const std::vector<ScoredRow>& hits)
{
    auto ordered = std::vector<GoItem> {};

    for (auto& hit: hits)
        ordered.push_back(hit.row);

    std::stable_sort(ordered.begin(),
                     ordered.end(),
                     [](const GoItem& a, const GoItem& b)
                     { return getSinkLevel(a) < getSinkLevel(b); });
    return ordered;
}

// Typed text that looks like a URL: the row already showing that exact URL
// goes first, else the row that opens it.
std::vector<GoItem> putURLActionFirst(std::vector<GoItem> rows,
                                      const std::string& query,
                                      PaletteMode mode)
{
    auto target = normalizeURL(query);
    auto exact = std::find_if(rows.begin(),
                              rows.end(),
                              [&](const GoItem& row) {
                                  return !isSinkingRow(row) && normalizeURL(row.url) == target;
                              });

    if (exact == rows.end())
        rows.insert(rows.begin(), makeTypedURLRow(query, mode));
    else
        std::rotate(rows.begin(), exact, exact + 1);

    return rows;
}

// The best hit that isn't failed or the current tab beats searching for the text.
bool isBestRowAStrongHit(const std::vector<ScoredRow>& hits)
{
    auto best = std::find_if(hits.begin(),
                             hits.end(),
                             [](const ScoredRow& hit) { return !isSinkingRow(hit.row); });

    return best != hits.end() && best->facts.level <= worstStrongMatchLevel;
}

// A history row for this very search would repeat the search row.
std::vector<GoItem> removePastSearches(std::vector<GoItem> rows, const std::string& query)
{
    auto searchURL = normalizeURL(navigationURL(query));

    std::erase_if(rows,
                  [&](const GoItem& row)
                  { return row.kind == GoKind::history && normalizeURL(row.url) == searchURL; });
    return rows;
}

// Typed words: a strong best hit stays first with search second; otherwise
// search goes first.
std::vector<GoItem> placeSearchRow(std::vector<GoItem> rows,
                                   const std::vector<ScoredRow>& hits,
                                   const std::string& query)
{
    auto searchPosition = isBestRowAStrongHit(hits) ? 1 : 0;
    rows.insert(rows.begin() + searchPosition, makeSearchRow(query));
    return rows;
}
} // namespace

std::vector<GoItem> listPaletteRows(const std::vector<PaletteTab>& tabs,
                                    const std::vector<Place>& places,
                                    const UsageFn& usage)
{
    auto starredPages = std::unordered_set<std::string> {};

    for (auto& place: places)
        starredPages.insert(pageKey(place.url));

    auto rows = std::vector<GoItem> {};
    auto current = std::vector<GoItem> {};

    for (auto& tab: sortTabsByLastViewed(tabs))
    {
        auto row = makeTabRow(tab, starredPages.contains(pageKey(tab.url)));
        auto& list = tab.isCurrent ? current : rows;
        list.push_back(std::move(row));
    }

    auto openPages = collectPageKeys(rows);
    openPages.merge(collectPageKeys(current));

    for (auto& place: listPlacesNotOpen(places, openPages, usage))
        rows.push_back(std::move(place));

    rows.insert(rows.end(), current.begin(), current.end());
    return rows;
}

std::vector<GoItem> rankPalette(const std::vector<GoItem>& rows,
                                const std::vector<HistoryEntry>& history,
                                const PaletteSearch& search)
{
    auto& query = search.query;
    auto& usage = search.usage;

    if (query.empty())
        return rows;

    auto candidates = appendHistoryRows(rows, history);

    if (looksLikeURL(query))
        return putURLActionFirst(orderHitsForDisplay(findHits(candidates, query, usage)), query, search.mode);

    auto hits = findHits(removePastSearches(std::move(candidates), query), query, usage);
    return placeSearchRow(orderHitsForDisplay(hits), hits, query);
}

int findDefaultSelection(const std::vector<GoItem>& rows)
{
    if (rows.empty() || rows.front().isCurrent)
        return -1;

    return 0;
}

int keepSelectionInRange(int selected, std::size_t rowCount)
{
    auto last = (int) rowCount - 1;
    return std::min(selected, last);
}

bool canCloseWithCtrlW(const GoItem& item)
{
    return item.kind == GoKind::tab;
}

PaletteOpening decidePaletteOpening(const GoItem& item, PaletteMode mode, bool inNewTab)
{
    if (item.tabId >= 0)
        return PaletteOpening::switchToTab;

    if (inNewTab || mode == PaletteMode::newTab)
        return PaletteOpening::openInNewTab;

    return PaletteOpening::loadInThisTab;
}

std::string completionText(const GoItem& item, const std::string& query)
{
    return item.kind == GoKind::search ? query : item.url;
}
} // namespace grazer
