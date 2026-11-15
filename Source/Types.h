#pragma once

#include <cstdint>
#include <string>
#include <vector>

// What a Go palette row is, shown on the row in words or an icon.
namespace GoKind
{
constexpr auto tab = "tab";
constexpr auto place = "place";
constexpr auto history = "history";
constexpr auto search = "search";
constexpr auto typedURL = "url";
} // namespace GoKind

// One row in the Go palette: an open tab (tabId >= 0), a starred place, a
// history page, or an action row for the typed text (search or open URL).
struct GoItem
{
    std::int64_t tabId = -1;
    std::string title;
    std::string url;
    bool bookmarked = false;
    std::string kind = GoKind::place;

    // The tab's last navigation failed (it shows an error page).
    bool failed = false;

    // The tab the palette was opened over. Listed last, never Enter's default.
    bool isCurrent = false;

    // Whether ✕ / ctrl-x / ctrl-q and ★ / ctrl-s do anything on this row.
    bool canClose = false;
    bool canStar = false;
};

// The palette's whole render model: PaletteView draws it, BrowserView does the
// filtering, ranking and selection. `generation` bumps each time the palette
// opens so the view resets its input to `query`.
struct GoResults
{
    std::vector<GoItem> items;

    // The highlighted row, or -1 for none (Enter then does nothing).
    std::int64_t selected = -1;
    std::int64_t generation = 0;

    // The search text the palette opened with: the current URL for ⌘L / o,
    // empty for ⌘T / t. The UI selects it all, so ⌘C copies it.
    std::string query;
};
