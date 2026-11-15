#pragma once

#include "History.h"
#include "Places.h"

#include <Types.h>

#include <functional>
#include <string>
#include <vector>

namespace grazer
{
// How much a page has been used lately (MruStore::usage), by normalized URL.
using UsageFn = std::function<double(const std::string& normalizedUrl)>;

// ⌘L / o opens the palette to go somewhere from the current tab; ⌘T / t opens
// it to make a new tab. Rows that aren't open tabs open accordingly.
enum class PaletteMode
{
    goFromHere,
    newTab
};

// An open tab as the palette lists it. Home tabs are never passed in.
struct PaletteTab
{
    std::int64_t tabId = -1;
    std::string title;
    std::string url;
    bool failed = false;
    std::int64_t lastViewedAtMs = 0;
    bool isCurrent = false;
};

// The palette's rows before any typing: open tabs by last viewed, then starred
// places that aren't open (most used lately first), then the current tab.
// An open starred place is one row, the tab, marked bookmarked.
std::vector<GoItem> listPaletteRows(const std::vector<PaletteTab>& tabs,
                                    const std::vector<Place>& places,
                                    const UsageFn& usage);

// What the user typed, with how rows will open and how much each page is used.
struct PaletteSearch
{
    std::string query;
    PaletteMode mode = PaletteMode::goFromHere;
    UsageFn usage;
};

// The rows for `search.query`. Empty: `rows` unchanged, no history. Otherwise row 0 is
// the best action: for URL-like text the exact tab/place/history match, else
// "Open <url>"; for words a strong hit with "Search Google for …" second, else
// search first. Hits are found and ordered by Rudder's FrecencySearch. Failed pages sink to the end, the current tab after them.
std::vector<GoItem> rankPalette(const std::vector<GoItem>& rows,
                                const std::vector<HistoryEntry>& history,
                                const PaletteSearch& search);

// The row highlighted for Enter: 0, or -1 when there are no rows or the only
// row is the current tab.
int findDefaultSelection(const std::vector<GoItem>& rows);

// The highlight after the list changed under it (a row closed, say): the same
// index, so the next row takes the closed one's place; the last row if the list
// got shorter than that; -1 stays -1.
int keepSelectionInRange(int selected, std::size_t rowCount);

// Whether ctrl-w closes this row: open tabs only.
bool canCloseWithCtrlW(const GoItem& item);

enum class PaletteOpening
{
    switchToTab,
    loadInThisTab,
    openInNewTab
};

// What choosing `item` does: an open tab is switched to; anything else loads in
// this tab in go-from-here mode, in a new tab in new-tab mode or with ⌘.
PaletteOpening decidePaletteOpening(const GoItem& item, PaletteMode mode, bool inNewTab);

// What Tab / → puts in the input for `item`, ready to keep typing: the row's
// URL, or for the search row the query it searches for.
std::string completionText(const GoItem& item, const std::string& query);
} // namespace grazer
