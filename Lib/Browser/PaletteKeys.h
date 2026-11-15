#pragma once

#include <SearchKeys/SearchKeys.h>

#include <optional>
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
} // namespace grazer
