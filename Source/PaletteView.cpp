#include "PaletteView.h"

#include <SessionLog/EventLine.h>

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

// The first UTF-8 character boundary at or after `length`.
std::size_t findCharacterStart(std::string_view text, std::size_t length)
{
    while (length < text.size() && isUtf8ContinuationByte(text[length]))
        ++length;

    return length;
}

// `text` cut to fit `width` in the painter's current font, with "…" after it
// when it was cut. Cuts land between UTF-8 characters. Binary search: each
// measurement shapes the whole prefix, so trying every length was quadratic.
std::string fitTextToWidth(UI::Graphics& g, std::string_view text, float width)
{
    if (g.measureText(text) <= width)
        return std::string {text};

    static constexpr auto ellipsis = std::string_view {"…"};
    auto available = width - g.measureText(ellipsis);
    auto fitting = std::size_t {0};
    auto tooLong = text.size();

    while (tooLong - fitting > 1)
    {
        auto halfway = fitting + (tooLong - fitting) / 2;
        auto middle = findCharacterStart(text, halfway);

        // No character starts in [halfway, tooLong): look below halfway.
        if (middle >= tooLong)
        {
            tooLong = halfway;
            continue;
        }

        if (g.measureText(text.substr(0, middle)) <= available)
            fitting = middle;
        else
            tooLong = middle;
    }

    return std::string {text.substr(0, fitting)} + std::string {ellipsis};
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

// eacp's TextEditor draws its text this far in from its left edge
// (editorTextInset in Widgets.cpp); the block caret has to land on the same
// glyphs.
constexpr auto editorTextInset = 6.f;
constexpr auto blockCaretAlpha = 0.55f;
constexpr auto blockCaretVerticalInset = 3.f;
constexpr auto modeLabelFontSize = 10.f;
constexpr auto modeLabelRightInset = 4.f;
// Far enough past either end of any list that moveSelection clamps to it.
constexpr auto pastEveryRow = 1'000'000;

// The character the block covers: the one after the caret, or a space at
// the end of the text so the block still has a width.
std::string getCharacterUnderCaret(const std::string& text, int caret)
{
    if (caret >= (int) text.size())
        return " ";

    auto start = (std::size_t) caret;
    auto next = findCharacterStart(text, start + 1);
    return text.substr(start, next - start);
}

// The editor's own rule: scroll only as far as it takes to keep the caret in view.
float scrollToShowCaret(float caretX, float visibleWidth, float currentScroll)
{
    if (visibleWidth <= 0.f)
        return currentScroll;

    return std::max(0.f, std::clamp(currentScroll, caretX - visibleWidth, caretX));
}

bool hasControlCharacter(const std::string& text)
{
    return std::any_of(text.begin(), text.end(), [](char c)
                       { return (unsigned char) c < 0x20 || c == 0x7f; });
}

// macOS gives arrows and function keys private-use code points, U+F700 to U+F8FF.
bool isFunctionKeyCharacter(const std::string& text)
{
    if (text.size() < 3 || (unsigned char) text[0] != 0xEF)
        return false;

    auto second = (unsigned char) text[1];
    return second >= 0x9C && second <= 0xA3;
}

bool isCommandOrControlHeld(const Graphics::ModifierKeys& modifiers)
{
    return modifiers.command || modifiers.control;
}

// What a key press types, if anything: no chords, no control characters and
// no function keys.
std::optional<std::string> getTypedText(const Graphics::KeyEvent& event)
{
    auto& text = event.characters;

    if (isCommandOrControlHeld(event.modifiers) || text.empty())
        return std::nullopt;

    if (hasControlCharacter(text) || isFunctionKeyCharacter(text))
        return std::nullopt;

    return text;
}

struct SpecialKeyBinding
{
    std::uint16_t keyCode;
    searchkeys::SpecialKey special;
};

const auto specialKeyBindings = std::vector<SpecialKeyBinding> {
    {Graphics::KeyCode::Escape, searchkeys::SpecialKey::escape},
    {Graphics::KeyCode::Return, searchkeys::SpecialKey::enter},
    {Graphics::KeyCode::UpArrow, searchkeys::SpecialKey::upArrow},
    {Graphics::KeyCode::DownArrow, searchkeys::SpecialKey::downArrow},
};

searchkeys::SpecialKey getSpecialKey(std::uint16_t keyCode)
{
    for (auto& binding: specialKeyBindings)
        if (binding.keyCode == keyCode)
            return binding.special;

    return searchkeys::SpecialKey::none;
}

searchkeys::Key makeSearchKey(const Graphics::KeyEvent& event)
{
    auto key = searchkeys::Key {};
    key.special = getSpecialKey(event.keyCode);
    key.characters = getTypedText(event).value_or("");
    key.command = event.modifiers.command;
    key.control = event.modifiers.control;
    key.alt = event.modifiers.alt;
    return key;
}

std::string_view getModeName(const searchkeys::State& state)
{
    if (searchkeys::isNormalMode(state))
        return "normal";

    return "insert";
}

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
    fittedTexts.assign(rows.size(), std::nullopt);
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
        auto visibleHeight = std::max(getHeight() - 2.f * listPadding, rowHeight);

        if (top < scroll)
            scroll = top;
        else if (top + rowHeight > scroll + visibleHeight)
            scroll = top + rowHeight - visibleHeight;
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

        paintRow(g, (std::size_t) index, {0.f, y, width, rowHeight}, index == highlighted);
    }
}

void PaletteRows::paintRow(UI::Graphics& g, std::size_t index, Rect area, bool isHighlighted)
{
    auto& row = rows[index];
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

    paintRowText(g, index, content);
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
void PaletteRows::resized()
{
    fittedTexts.assign(rows.size(), std::nullopt);
}

// Fitting measures text, which shapes it; done once per row until the rows or the width change.
const FittedRowText& PaletteRows::getFittedRowText(UI::Graphics& g, std::size_t index, float width)
{
    auto& fitted = fittedTexts[index];

    if (fitted)
        return *fitted;

    auto& row = rows[index];
    auto title = row.title.empty() ? row.url : row.title;
    fitted = FittedRowText {};
    g.setFontSize(titleFontSize);
    fitted->title = fitTextToWidth(g, title, width);

    if (!isActionRow(row))
    {
        g.setFontSize(detailFontSize);
        fitted->url = fitTextToWidth(g, row.url, width);
    }

    return *fitted;
}

void PaletteRows::paintRowText(UI::Graphics& g, std::size_t index, Rect content)
{
    auto& row = rows[index];
    auto& fitted = getFittedRowText(g, index, content.w);
    g.setFontSize(titleFontSize);

    if (isActionRow(row))
    {
        g.setColour(actionTextColour);
        g.drawText(fitted.title, content);
        return;
    }

    g.setColour(fadeIfFailed(textColour, row));
    g.drawText(fitted.title, content.withHeight(rowHeight * titleShare));

    g.setFontSize(detailFontSize);
    g.setColour(fadeIfFailed(dimTextColour, row));
    g.drawText(fitted.url,
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
    queryFont.family = queryFontFamily;
    queryFont.pointSize = queryFontSize;

    query.setFont(queryFont);
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

void PalettePanel::setNormalMode(bool isNormal)
{
    if (normalMode == isNormal)
        return;

    normalMode = isNormal;
    // The thin caret is drawn in the accent colour: hidden under the block.
    query.setAccentColour(isNormal ? Color::transparent() : UI::defaultTheme().accent);
    repaint();
}

void PalettePanel::paintOverChildren(UI::Graphics& g)
{
    if (!normalMode)
        return;

    paintBlockCaret(g);
    paintModeLabel(g);
}

void PalettePanel::paintBlockCaret(UI::Graphics& g)
{
    auto scope = UI::Graphics::ScopedState {g};
    auto field = query.getBounds();
    g.setFont(queryFont);

    auto& content = query.getText();
    auto caret = std::clamp(query.getCaretPosition(), 0, (int) content.size());
    auto textBeforeCaret = std::string_view(content).substr(0, (std::size_t) caret);
    auto caretX = g.measureText(textBeforeCaret);
    auto visibleWidth = field.w - editorTextInset * 2.f;
    blockScroll = scrollToShowCaret(caretX, visibleWidth, blockScroll);

    auto blockWidth = g.measureText(getCharacterUnderCaret(content, caret));
    auto left = field.x + editorTextInset - blockScroll + caretX;

    g.setColour(textColour.withAlpha(blockCaretAlpha));
    g.fillRect({left,
                field.y + blockCaretVerticalInset,
                blockWidth,
                field.h - blockCaretVerticalInset * 2.f});
}

void PalettePanel::paintModeLabel(UI::Graphics& g)
{
    auto scope = UI::Graphics::ScopedState {g};
    auto field = query.getBounds();
    auto labelFont = queryFont;
    labelFont.pointSize = modeLabelFontSize;
    g.setFont(labelFont);

    constexpr auto label = std::string_view("NORMAL");
    auto labelWidth = g.measureText(label);
    auto baseline = field.y + (field.h - g.lineHeight()) * 0.5f + g.ascent();

    g.setColour(dimTextColour);
    g.drawText(label, {field.x + field.w - labelWidth - modeLabelRightInset, baseline});
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
    panel.query.onTextChange = [this](const std::string& typedText) { onSetQuery(typedText); };
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
        searchKeysState = searchkeys::getOpeningState();
        panel.setNormalMode(false);
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
    auto completedText = onComplete();

    if (completedText.empty())
        return;

    backdrop.panel.query.setText(completedText);
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
        {PaletteKeyAction::moveToFirst, [this] { onMoveSelection(-pastEveryRow); }},
        {PaletteKeyAction::moveToLast, [this] { onMoveSelection(pastEveryRow); }},
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

// Vim modes from Rudder's SearchKeys come first; a key they pass through is
// handled as before modes existed.
bool PaletteView::handleSearchKey(const Graphics::KeyEvent& event)
{
    auto& query = backdrop.panel.query;
    auto current = searchKeysState;
    current.text = query.getText();
    current.caret = query.getCaretPosition();

    auto result = searchkeys::handleKey(current, makeSearchKey(event));

    if (result.action == searchkeys::Action::passThrough)
        return false;

    applySearchState(result.state);

    if (auto action = grazer::getPaletteKeyAction(result.action))
    {
        LOG(sessionlog::formatEventLine(
            "palette.key",
            {{"mode", getModeName(result.state)},
             {"action", grazer::getPaletteKeyActionName(*action)}}));
        runPaletteKeyAction(*action);
    }

    return true;
}

void PaletteView::applySearchState(const searchkeys::State& state)
{
    auto& panel = backdrop.panel;

    if (searchkeys::isNormalMode(state) != searchkeys::isNormalMode(searchKeysState))
        LOG(sessionlog::formatEventLine("palette.mode", {{"mode", getModeName(state)}}));

    // A text change re-filters exactly as typing does.
    if (state.text != panel.query.getText())
        panel.query.setText(state.text, true);

    panel.query.setCaretPosition(state.caret);
    panel.setNormalMode(searchkeys::isNormalMode(state));
    searchKeysState = state;
}

void PaletteView::keyDown(const Graphics::KeyEvent& event)
{
    if (handleSearchKey(event))
        return;

    if (auto action = findPaletteKeyAction(event))
        runPaletteKeyAction(*action);
    else
        ComponentHost::keyDown(event);
}
