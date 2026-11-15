#pragma once

#include <Miro/Bridge.h>
#include <Miro/Reflect.h>

#include <cstdint>
#include <functional>
#include <string>

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

    // Whether ✕ / ctrl-x and ★ / ctrl-s do anything on this row.
    bool canClose = false;
    bool canStar = false;

    MIRO_REFLECT(tabId, title, url, bookmarked, kind, failed, isCurrent, canClose, canStar)
};

// The palette's whole render model. Filtering, ranking and selection are
// native; the web UI just draws this and forwards input. `generation` bumps
// each time the palette opens so the UI can reset its input to `query`.
struct GoResults
{
    std::vector<GoItem> items;

    // The highlighted row, or -1 for none (Enter then does nothing).
    std::int64_t selected = -1;
    std::int64_t generation = 0;

    // The search text the palette opened with: the current URL for ⌘L / o,
    // empty for ⌘T / t. The UI selects it all, so ⌘C copies it.
    std::string query;

    MIRO_REFLECT(items, selected, generation, query)
};

struct GoOpen
{
    bool inNewTab = false;

    MIRO_REFLECT(inNewTab)
};

struct GoChoice
{
    GoItem item;
    bool inNewTab = false;

    MIRO_REFLECT(item, inNewTab)
};

struct GoQuery
{
    std::string text;

    MIRO_REFLECT(text)
};

struct GoDelta
{
    std::int64_t delta = 0;

    MIRO_REFLECT(delta)
};

namespace Api
{

// The palette's wire surface. The web UI is a dumb renderer: it draws
// `results` and forwards keystrokes as commands; all matching and selection
// logic runs natively. Behaviour is injected by BrowserView through
// the std::function members -- the codegen executable default-constructs this
// class, so it must stay header-only and default-constructible.
class GrazerApi
{
public:
    void reflect(Miro::ApiReflector& r)
    {
        using T = GrazerApi;

        r.commands<&T::getResults,
                   &T::setQuery,
                   &T::moveSelection,
                   &T::activate,
                   &T::complete,
                   &T::choose,
                   &T::cancel,
                   &T::closeItem,
                   &T::closeTabRow,
                   &T::toggleBookmark>();
        r.events<&T::results>();
    }

    GoResults getResults() const { return results.snapshot(); }

    void setQuery(const GoQuery& query) { onSetQuery(query.text); }
    void moveSelection(const GoDelta& delta) { onMoveSelection((int) delta.delta); }
    void activate(const GoOpen& open) { onActivate(open.inNewTab); }
    GoQuery complete() { return {onComplete()}; }
    void choose(const GoChoice& choice) { onChoose(choice.item, choice.inNewTab); }
    void cancel() { onCancel(); }
    void closeItem(const GoItem& item) { onCloseItem(item); }
    void closeTabRow(const GoItem& item) { onCloseTabRow(item); }
    void toggleBookmark(const GoItem& item) { onToggleBookmark(item); }

    Miro::Event<GoResults> results;

    std::function<void(const std::string&)> onSetQuery = [](auto&) {};
    std::function<void(int)> onMoveSelection = [](int) {};
    std::function<void(bool inNewTab)> onActivate = [](bool) {};
    std::function<std::string()> onComplete = [] { return std::string {}; };
    std::function<void(const GoItem&, bool inNewTab)> onChoose = [](auto&, bool) {};
    std::function<void()> onCancel = [] {};
    std::function<void(const GoItem&)> onCloseItem = [](auto&) {};
    std::function<void(const GoItem&)> onCloseTabRow = [](auto&) {};
    std::function<void(const GoItem&)> onToggleBookmark = [](auto&) {};
};

} // namespace Api
