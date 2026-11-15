#include "PaletteView.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string_view>

using namespace eacp;
using eacp::Graphics::Color;
using eacp::Graphics::Rect;

namespace
{
const auto backdropColour = Color(0.f, 0.f, 0.f, 0.35f);
const auto panelColour = Color(0.094f, 0.094f, 0.11f, 1.f);
const auto panelEdgeColour = Color::white(0.12f);
const auto panelShadowColour = Color::black(0.55f);
const auto dividerColour = Color::white(0.10f);
const auto textColour = Color(0.95f, 0.95f, 0.96f, 1.f);
const auto actionTextColour = Color::white(0.75f);
const auto dimTextColour = Color::white(0.45f);
const auto emptyTextColour = Color::white(0.5f);
const auto markerColour = Color::white(0.55f);
const auto buttonColour = Color::white(0.35f);
const auto highlightColour = Color(0.35f, 0.51f, 1.f, 0.28f);
const auto openTabColour = Color(0.39f, 0.83f, 0.49f, 1.f);
const auto starColour = Color(1.f, 0.84f, 0.43f, 1.f);
const auto failedColour = Color(0.88f, 0.39f, 0.36f, 1.f);

// A failed tab's title and URL, faded behind its red marker.
constexpr auto failedTextOpacity = 0.55f;

constexpr auto panelMaximumWidth = 680.f;
constexpr auto panelSideMargin = 30.f;
constexpr auto panelTopShare = 0.14f;
constexpr auto panelMaximumHeightShare = 0.70f;
constexpr auto panelBottomMargin = 30.f;
constexpr auto panelCornerRadius = 14.f;
constexpr auto panelShadowOffset = 24.f;
constexpr auto panelShadowBlur = 70.f;

constexpr auto queryFontFamily = "Helvetica Neue";
constexpr auto queryFontSize = 16.f;
constexpr auto queryInsetX = 18.f;
constexpr auto queryInsetY = 13.f;

constexpr auto rowInsetX = 14.f;
constexpr auto highlightInset = 6.f;
constexpr auto highlightCornerRadius = 7.f;
constexpr auto markerWidth = 16.f;
constexpr auto columnGap = 10.f;
constexpr auto buttonWidth = 24.f;
constexpr auto kindLabelGap = 8.f;
constexpr auto titleFontSize = 14.f;
constexpr auto detailFontSize = 11.f;
constexpr auto emptyFontSize = 13.f;
constexpr auto titleShare = 0.56f;
constexpr auto urlShare = 0.40f;
constexpr auto urlRaise = 2.f;

// Room above and below the list inside the panel.
constexpr auto listPadding = 4.f;

struct RowKindMarker
{
    std::string_view glyph;
    std::string_view label;
    Color colour;
};

// The row's kind, in a glyph and a word, so rows of different kinds never look alike.
RowKindMarker getRowKindMarker(const GoItem& row)
{
    if (row.failed)
        return {"!", "Didn't load", failedColour};

    if (row.isCurrent)
        return {"▣", "Current", openTabColour};

    if (row.kind == GoKind::tab)
        return {"▣", "Switch to tab", openTabColour};

    if (row.kind == GoKind::place)
        return {"★", "Starred", starColour};

    if (row.kind == GoKind::history)
        return {"◷", "History", markerColour};

    if (row.kind == GoKind::typedURL)
        return {"🌐", "Open", markerColour};

    return {"⌕", "Search", markerColour};
}

bool isActionRow(const GoItem& row)
{
    return row.kind == GoKind::search || row.kind == GoKind::typedURL;
}

bool isUtf8ContinuationByte(char c)
{
    return ((unsigned char) c & 0xC0) == 0x80;
}

// `text` cut to fit `width` in the painter's current font, with "…" after it
// when it was cut. Cuts land between UTF-8 characters.
std::string fitTextToWidth(UI::Graphics& g, std::string_view text, float width)
{
    if (g.measureText(text) <= width)
        return std::string {text};

    static constexpr auto ellipsis = std::string_view {"…"};
    auto available = width - g.measureText(ellipsis);
    auto length = text.size();

    while (length > 0)
    {
        do
            --length;
        while (length > 0 && isUtf8ContinuationByte(text[length]));

        if (g.measureText(text.substr(0, length)) <= available)
            break;
    }

    return std::string {text.substr(0, length)} + std::string {ellipsis};
}

Color fadeIfFailed(Color colour, const GoItem& row)
{
    if (row.failed)
        return colour.withAlpha(colour.a * failedTextOpacity);

    return colour;
}

Rect getStarButtonArea(Rect row)
{
    return row.withX(row.right() - rowInsetX - 2.f * buttonWidth).withWidth(buttonWidth);
}

Rect getCloseButtonArea(Rect row)
{
    return row.withX(row.right() - rowInsetX - buttonWidth).withWidth(buttonWidth);
}

bool isNewTabModifierHeld(const Graphics::ModifierKeys& modifiers)
{
    return modifiers.command || modifiers.control;
}

bool isPlainRightArrow(const Graphics::KeyEvent& event)
{
    return event.keyCode == Graphics::KeyCode::RightArrow && !event.modifiers.shift;
}

bool isOnlyControlHeld(const Graphics::ModifierKeys& modifiers)
{
    return modifiers.control && !modifiers.command && !modifiers.alt;
}

bool canStarRow(const GoItem& row)
{
    return row.canStar;
}

bool canCloseRow(const GoItem& row)
{
    return row.canClose;
}

bool isOpenTabRow(const GoItem& row)
{
    return row.kind == GoKind::tab;
}

struct KeyBinding
{
    std::uint16_t keyCode;
    PaletteKeyAction action;
};

const auto plainKeyActions = std::vector<KeyBinding> {
    {Graphics::KeyCode::Return, PaletteKeyAction::activate},
    {Graphics::KeyCode::Escape, PaletteKeyAction::cancel},
    {Graphics::KeyCode::DownArrow, PaletteKeyAction::moveDown},
    {Graphics::KeyCode::UpArrow, PaletteKeyAction::moveUp},
    {Graphics::KeyCode::Tab, PaletteKeyAction::complete},
};

const auto controlKeyActions = std::vector<KeyBinding> {
    {Graphics::KeyCode::J, PaletteKeyAction::moveDown},
    {Graphics::KeyCode::N, PaletteKeyAction::moveDown},
    {Graphics::KeyCode::K, PaletteKeyAction::moveUp},
    {Graphics::KeyCode::P, PaletteKeyAction::moveUp},
    {Graphics::KeyCode::S, PaletteKeyAction::toggleStar},
    {Graphics::KeyCode::X, PaletteKeyAction::closeRow},
    {Graphics::KeyCode::W, PaletteKeyAction::closeTabRow},
};

std::optional<PaletteKeyAction> findKeyAction(const std::vector<KeyBinding>& bindings,
                                              std::uint16_t keyCode)
{
    for (auto& binding: bindings)
        if (binding.keyCode == keyCode)
            return binding.action;

    return std::nullopt;
}
} // namespace

// --- PaletteRows -----------------------------------------------------------------

PaletteRows::PaletteRows()
{
    setInterceptsMouseClicks(true);
}

void PaletteRows::setRows(const std::vector<GoItem>& newRows, int newHighlighted)
{
    rows = newRows;
    highlighted = newHighlighted;
    scrollToHighlightedRow();
    repaint();
}

float PaletteRows::getContentHeight() const
{
    auto shownRows = std::max<std::size_t>(rows.size(), 1);
    return (float) shownRows * rowHeight + 2.f * listPadding;
}

float PaletteRows::getMaximumScroll() const
{
    return std::max(0.f, getContentHeight() - getHeight());
}

void PaletteRows::scrollToHighlightedRow()
{
    if (highlighted >= 0)
    {
        auto top = (float) highlighted * rowHeight;
        auto visible = std::max(getHeight() - 2.f * listPadding, rowHeight);

        if (top < scroll)
            scroll = top;
        else if (top + rowHeight > scroll + visible)
            scroll = top + rowHeight - visible;
    }

    scroll = std::clamp(scroll, 0.f, getMaximumScroll());
}

int PaletteRows::findRowAt(float y) const
{
    auto index = (int) ((y - listPadding + scroll) / rowHeight);

    if (y < listPadding || index >= (int) rows.size())
        return -1;

    return index;
}

void PaletteRows::paint(UI::Graphics& g)
{
    auto width = getWidth();

    if (rows.empty())
    {
        g.setColour(emptyTextColour);
        g.setFontSize(emptyFontSize);
        g.drawText("Nothing here yet", {queryInsetX, listPadding, width - 2.f * queryInsetX, rowHeight});
        return;
    }

    for (auto index = 0; index < (int) rows.size(); ++index)
    {
        auto y = listPadding + (float) index * rowHeight - scroll;

        if (y + rowHeight < 0.f || y > getHeight())
            continue;

        paintRow(g, rows[(std::size_t) index], {0.f, y, width, rowHeight}, index == highlighted);
    }
}

void PaletteRows::paintRow(UI::Graphics& g, const GoItem& row, Rect area, bool isHighlighted)
{
    if (isHighlighted)
    {
        g.setColour(highlightColour);
        g.fillRoundedRect(area.inset(highlightInset, 0.f), highlightCornerRadius);
    }

    auto marker = getRowKindMarker(row);
    auto content = area.inset(rowInsetX, 0.f);

    g.setColour(marker.colour);
    g.setFontSize(titleFontSize);
    g.drawText(marker.glyph, content.removeFromLeft(markerWidth), UI::Justification::Centred);
    content.removeFromLeft(columnGap);

    paintRowButtons(g, row, area);
    content.removeFromRight(2.f * buttonWidth);

    g.setFontSize(detailFontSize);
    g.setColour(row.failed ? failedColour : dimTextColour);
    g.drawText(marker.label, content.removeFromRight(g.measureText(marker.label)), UI::Justification::Right);
    content.removeFromRight(kindLabelGap);

    paintRowText(g, row, content);
}

void PaletteRows::paintRowButtons(UI::Graphics& g, const GoItem& row, Rect area)
{
    g.setFontSize(titleFontSize);

    if (row.canClose)
    {
        g.setColour(buttonColour);
        g.drawText("✕", getCloseButtonArea(area), UI::Justification::Centred);
    }

    if (row.canStar)
    {
        g.setColour(row.bookmarked ? starColour : buttonColour);
        g.drawText(row.bookmarked ? "★" : "☆", getStarButtonArea(area), UI::Justification::Centred);
    }
}

// Title over a grey URL; an action row (search, open a typed URL) is one line.
void PaletteRows::paintRowText(UI::Graphics& g, const GoItem& row, Rect content)
{
    auto title = row.title.empty() ? row.url : row.title;
    g.setFontSize(titleFontSize);

    if (isActionRow(row))
    {
        g.setColour(actionTextColour);
        g.drawText(fitTextToWidth(g, title, content.w), content);
        return;
    }

    g.setColour(fadeIfFailed(textColour, row));
    g.drawText(fitTextToWidth(g, title, content.w), content.withHeight(rowHeight * titleShare));

    g.setFontSize(detailFontSize);
    g.setColour(fadeIfFailed(dimTextColour, row));
    g.drawText(fitTextToWidth(g, row.url, content.w),
               content.withY(content.y + rowHeight * titleShare - urlRaise).withHeight(rowHeight * urlShare));
}

void PaletteRows::mouseDown(const UI::MouseEvent& event)
{
    auto index = findRowAt(event.position.y);

    if (index < 0)
        return;

    auto& row = rows[(std::size_t) index];
    auto area = Rect {0.f, listPadding + (float) index * rowHeight - scroll, getWidth(), rowHeight};

    if (row.canClose && getCloseButtonArea(area).contains(event.position))
        onClose(row);
    else if (row.canStar && getStarButtonArea(area).contains(event.position))
        onToggleStar(row);
    else
        onChoose(row, isNewTabModifierHeld(event.modifiers));
}

bool PaletteRows::mouseWheelMove(const UI::MouseEvent& event)
{
    auto step = event.preciseWheel ? event.wheelDelta.y : event.wheelDelta.y * rowHeight;
    scroll = std::clamp(scroll - step, 0.f, getMaximumScroll());
    repaint();
    return true;
}

// --- PalettePanel ----------------------------------------------------------------

PalettePanel::PalettePanel()
{
    auto font = Text::Font {};
    font.family = queryFontFamily;
    font.pointSize = queryFontSize;

    query.setFont(font);
    query.setDrawsFrame(false);
    query.setPlaceholder("Go to a place, tab, search, or URL…");
    query.setColour(textColour);
    query.setWantsKeyboardFocus(true);

    addChildren({query, rows});
}

void PalettePanel::resized()
{
    auto area = getLocalBounds();
    query.setBounds(area.removeFromTop(queryHeight).inset(queryInsetX, queryInsetY));
    rows.setBounds(area);
}

void PalettePanel::paint(UI::Graphics& g)
{
    g.setColour(panelColour);
    g.fillRoundedRect(getLocalBounds(), panelCornerRadius);
    g.setColour(panelEdgeColour);
    g.drawRoundedRect(getLocalBounds(), panelCornerRadius);
    g.setColour(dividerColour);
    g.fillRect({0.f, queryHeight - 1.f, getWidth(), 1.f});
}

// --- PaletteBackdrop -------------------------------------------------------------

PaletteBackdrop::PaletteBackdrop()
{
    setInterceptsMouseClicks(true);
    addChildren({panel});
}

Rect PaletteBackdrop::calculatePanelBounds() const
{
    auto width = std::min(panelMaximumWidth, getWidth() - 2.f * panelSideMargin);
    auto top = getHeight() * panelTopShare;
    auto maximumHeight =
        std::min(getHeight() * panelMaximumHeightShare, getHeight() - top - panelBottomMargin);
    auto height = std::min(maximumHeight, PalettePanel::queryHeight + panel.rows.getContentHeight());

    return {(getWidth() - width) / 2.f, top, std::max(width, 0.f), std::max(height, 0.f)};
}

void PaletteBackdrop::resized()
{
    panel.setBounds(calculatePanelBounds());
}

void PaletteBackdrop::paint(UI::Graphics& g)
{
    g.fillAll(backdropColour);

    auto shadow = UI::ShadowShape {};
    shadow.box = calculatePanelBounds();
    shadow.cornerRadius = panelCornerRadius;
    shadow.offset = {0.f, panelShadowOffset};
    shadow.blurRadius = panelShadowBlur;
    g.setColour(panelShadowColour);
    g.drawShadow(shadow);
}

void PaletteBackdrop::mouseDown(const UI::MouseEvent&)
{
    onCancel();
}

// --- PaletteView -----------------------------------------------------------------

PaletteView::PaletteView()
{
    setTransparent(true);
    setBackgroundColour(Color::transparent());
    setTabMovesFocus(false);
    setRootComponent(backdrop);

    auto& panel = backdrop.panel;
    panel.query.onTextChange = [this](const std::string& text) { onSetQuery(text); };
    panel.rows.onChoose = [this](const GoItem& row, bool inNewTab) { onChoose(row, inNewTab); };
    panel.rows.onToggleStar = [this](const GoItem& row) { onToggleBookmark(row); };
    panel.rows.onClose = [this](const GoItem& row) { onCloseItem(row); };
    backdrop.onCancel = [this] { onCancel(); };
}

void PaletteView::setResults(const GoResults& results)
{
    shown = results;

    auto& panel = backdrop.panel;
    panel.rows.setRows(shown.items, (int) shown.selected);

    if (shown.generation != shownGeneration)
    {
        shownGeneration = shown.generation;
        panel.query.setText(shown.query);
        panel.query.selectAll();
    }

    backdrop.resized();
    backdrop.repaint();
}

void PaletteView::focusQuery()
{
    focus();
    backdrop.panel.query.grabKeyboardFocus();
}

std::optional<GoItem> PaletteView::getHighlightedRow() const
{
    if (shown.selected < 0 || shown.selected >= (std::int64_t) shown.items.size())
        return std::nullopt;

    return shown.items[(std::size_t) shown.selected];
}

bool PaletteView::isCaretAtEnd() const
{
    auto& query = backdrop.panel.query;
    return !query.hasSelection() && query.getCaretPosition() == (int) query.getText().size();
}

// Tab / →: the highlighted row's URL goes into the input, caret at its end.
void PaletteView::completeHighlightedRow()
{
    auto text = onComplete();

    if (text.empty())
        return;

    backdrop.panel.query.setText(text);
}

// The palette's own keys, before the query field sees them.
std::optional<PaletteKeyAction> PaletteView::findPaletteKeyAction(const Graphics::KeyEvent& event) const
{
    auto& modifiers = event.modifiers;

    if (event.keyCode == Graphics::KeyCode::Return && isNewTabModifierHeld(modifiers))
        return PaletteKeyAction::activateInNewTab;

    if (auto plain = findKeyAction(plainKeyActions, event.keyCode))
        return plain;

    if (isPlainRightArrow(event) && isCaretAtEnd())
        return PaletteKeyAction::complete;

    if (isOnlyControlHeld(modifiers))
        return findKeyAction(controlKeyActions, event.keyCode);

    return std::nullopt;
}

void PaletteView::runOnHighlightedRow(bool (*isAllowed)(const GoItem&),
                                      const std::function<void(const GoItem&)>& action)
{
    if (auto row = getHighlightedRow(); row && isAllowed(*row))
        action(*row);
}

void PaletteView::runPaletteKeyAction(PaletteKeyAction action)
{
    auto handlers = std::map<PaletteKeyAction, std::function<void()>> {
        {PaletteKeyAction::moveDown, [this] { onMoveSelection(1); }},
        {PaletteKeyAction::moveUp, [this] { onMoveSelection(-1); }},
        {PaletteKeyAction::toggleStar, [this] { runOnHighlightedRow(canStarRow, onToggleBookmark); }},
        {PaletteKeyAction::closeRow, [this] { runOnHighlightedRow(canCloseRow, onCloseItem); }},
        {PaletteKeyAction::closeTabRow, [this] { runOnHighlightedRow(isOpenTabRow, onCloseTabRow); }},
        {PaletteKeyAction::complete, [this] { completeHighlightedRow(); }},
        {PaletteKeyAction::activate, [this] { onActivate(false); }},
        {PaletteKeyAction::activateInNewTab, [this] { onActivate(true); }},
        {PaletteKeyAction::cancel, [this] { onCancel(); }},
    };

    handlers.at(action)();
}

void PaletteView::keyDown(const Graphics::KeyEvent& event)
{
    if (auto action = findPaletteKeyAction(event))
        runPaletteKeyAction(*action);
    else
        ComponentHost::keyDown(event);
}
