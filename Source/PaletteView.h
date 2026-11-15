#pragma once

#include "Types.h"

#include <eacp/UI/UI.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

// What a key does in the palette before the query field sees it.
enum class PaletteKeyAction
{
    moveDown,
    moveUp,
    toggleStar,
    closeRow,
    closeTabRow,
    complete,
    activate,
    activateInNewTab,
    cancel,
};

// The Go palette's rows: kind marker, title over a grey URL, the kind in words,
// and ★ / ✕ on the right. Painted by hand, the way Rudder's ResultsList is, so
// a row is exactly the shape the palette wants and only the list repaints when
// the highlight moves.
class PaletteRows final : public eacp::UI::Component
{
public:
    PaletteRows();

    void setRows(const std::vector<GoItem>& newRows, int newHighlighted);
    float getContentHeight() const;

    void paint(eacp::UI::Graphics& g) override;
    void mouseDown(const eacp::UI::MouseEvent& event) override;
    bool mouseWheelMove(const eacp::UI::MouseEvent& event) override;

    std::function<void(const GoItem&, bool inNewTab)> onChoose = [](auto&, bool) {};
    std::function<void(const GoItem&)> onToggleStar = [](auto&) {};
    std::function<void(const GoItem&)> onClose = [](auto&) {};

    static constexpr auto rowHeight = 46.f;

private:
    void scrollToHighlightedRow();
    float getMaximumScroll() const;
    int findRowAt(float y) const;
    void paintRow(eacp::UI::Graphics& g, const GoItem& row, eacp::Graphics::Rect area, bool isHighlighted);
    void paintRowButtons(eacp::UI::Graphics& g, const GoItem& row, eacp::Graphics::Rect area);
    void paintRowText(eacp::UI::Graphics& g, const GoItem& row, eacp::Graphics::Rect content);

    std::vector<GoItem> rows;
    int highlighted = -1;
    float scroll = 0.f;
};

// The rounded panel: the query field on top, the rows under it.
class PalettePanel final : public eacp::UI::Component
{
public:
    PalettePanel();

    void paint(eacp::UI::Graphics& g) override;
    void resized() override;

    eacp::UI::TextEditor query;
    PaletteRows rows;

    static constexpr auto queryHeight = 52.f;
};

// The whole window while the palette is open: a dim cover over the page (a
// click on it cancels) with the panel near the top, centred.
class PaletteBackdrop final : public eacp::UI::Component
{
public:
    PaletteBackdrop();

    void paint(eacp::UI::Graphics& g) override;
    void resized() override;
    void mouseDown(const eacp::UI::MouseEvent& event) override;

    PalettePanel panel;
    std::function<void()> onCancel = [] {};

private:
    eacp::Graphics::Rect calculatePanelBounds() const;
};

// The Go palette, drawn natively over the tabs: one ComponentHost (a GPUView)
// that draws GoResults and reports input. Ranking, highlight and what a row
// opens all stay with BrowserView.
class PaletteView final : public eacp::UI::ComponentHost
{
public:
    PaletteView();

    // A new `generation` is a fresh open: the input takes `query`, all of it
    // selected, so ⌘C copies it and typing replaces it.
    void setResults(const GoResults& results);

    // Key focus into the query field.
    void focusQuery();

    void keyDown(const eacp::Graphics::KeyEvent& event) override;

    std::function<void(const std::string&)> onSetQuery = [](auto&) {};
    std::function<void(int)> onMoveSelection = [](int) {};
    std::function<void(bool inNewTab)> onActivate = [](bool) {};
    std::function<std::string()> onComplete = [] { return std::string {}; };
    std::function<void(const GoItem&, bool inNewTab)> onChoose = [](auto&, bool) {};
    std::function<void()> onCancel = [] {};
    std::function<void(const GoItem&)> onCloseItem = [](auto&) {};
    std::function<void(const GoItem&)> onCloseTabRow = [](auto&) {};
    std::function<void(const GoItem&)> onToggleBookmark = [](auto&) {};

private:
    std::optional<PaletteKeyAction> findPaletteKeyAction(const eacp::Graphics::KeyEvent& event) const;
    void runPaletteKeyAction(PaletteKeyAction action);
    void runOnHighlightedRow(bool (*isAllowed)(const GoItem&),
                             const std::function<void(const GoItem&)>& action);
    bool isCaretAtEnd() const;
    void completeHighlightedRow();
    std::optional<GoItem> getHighlightedRow() const;

    PaletteBackdrop backdrop;
    GoResults shown;
    std::int64_t shownGeneration = -1;
};
