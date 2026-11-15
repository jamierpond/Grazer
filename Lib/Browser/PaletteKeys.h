#pragma once

#include <SearchKeys/SearchKeys.h>

#include <optional>
#include <string>
#include <string_view>

namespace grazer
{
// What a key does in the palette before the query field sees it.
enum class PaletteKeyAction
{
    moveDown,
    moveUp,
    moveToFirst,
    moveToLast,
    toggleStar,
    closeRow,
    complete,
    activate,
    activateInNewTab,
    cancel,
};

// The palette outcome for a list action from the SearchKeys search box; none
// for passThrough (the palette's own keys, then the query field) and none (a text edit).
std::optional<PaletteKeyAction> getPaletteKeyAction(searchkeys::Action action);

std::string_view getPaletteKeyActionName(PaletteKeyAction action);

// The parts of a key press that decide what the search box is told it types.
struct PressedKeyText
{
    std::string characters;
    std::string charactersIgnoringModifiers;
    bool command = false;
    bool control = false;
};

// What the search box is told a key types: the letter of a control chord
// (ctrl+w is "w", so the box can delete a word), nothing for command chords,
// control characters or function keys, else the typed text.
std::string getSearchKeyCharacters(const PressedKeyText& key);
} // namespace grazer
