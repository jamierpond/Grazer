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
    closeTabRow,
    complete,
    activate,
    activateInNewTab,
    cancel,
};

// The palette outcome for a vim-mode action from SearchKeys; none for
// passThrough (the key is handled as before modes existed) and none.
std::optional<PaletteKeyAction> getPaletteKeyAction(searchkeys::Action action);

std::string_view getPaletteKeyActionName(PaletteKeyAction action);
} // namespace grazer
