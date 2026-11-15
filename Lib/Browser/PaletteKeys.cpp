#include "PaletteKeys.h"

#include <array>

namespace grazer
{
namespace
{
struct SearchActionOutcome
{
    searchkeys::Action searchAction;
    PaletteKeyAction paletteAction;
};

constexpr auto searchActionOutcomes = std::array {
    SearchActionOutcome {searchkeys::Action::moveHighlightDown, PaletteKeyAction::moveDown},
    SearchActionOutcome {searchkeys::Action::moveHighlightUp, PaletteKeyAction::moveUp},
    SearchActionOutcome {searchkeys::Action::moveHighlightToFirst, PaletteKeyAction::moveToFirst},
    SearchActionOutcome {searchkeys::Action::moveHighlightToLast, PaletteKeyAction::moveToLast},
    SearchActionOutcome {searchkeys::Action::openHighlighted, PaletteKeyAction::activate},
    SearchActionOutcome {searchkeys::Action::openHighlightedInNewTab,
                         PaletteKeyAction::activateInNewTab},
    SearchActionOutcome {searchkeys::Action::starHighlighted, PaletteKeyAction::toggleStar},
    SearchActionOutcome {searchkeys::Action::closeSearch, PaletteKeyAction::cancel},
};

struct PaletteKeyActionName
{
    PaletteKeyAction action;
    std::string_view name;
};

constexpr auto paletteKeyActionNames = std::array {
    PaletteKeyActionName {PaletteKeyAction::moveDown, "move down"},
    PaletteKeyActionName {PaletteKeyAction::moveUp, "move up"},
    PaletteKeyActionName {PaletteKeyAction::moveToFirst, "move to first"},
    PaletteKeyActionName {PaletteKeyAction::moveToLast, "move to last"},
    PaletteKeyActionName {PaletteKeyAction::toggleStar, "toggle star"},
    PaletteKeyActionName {PaletteKeyAction::closeRow, "close row"},
    PaletteKeyActionName {PaletteKeyAction::complete, "complete"},
    PaletteKeyActionName {PaletteKeyAction::activate, "open"},
    PaletteKeyActionName {PaletteKeyAction::activateInNewTab, "open in new tab"},
    PaletteKeyActionName {PaletteKeyAction::cancel, "cancel"},
};
} // namespace

std::optional<PaletteKeyAction> getPaletteKeyAction(searchkeys::Action action)
{
    for (auto& outcome: searchActionOutcomes)
        if (outcome.searchAction == action)
            return outcome.paletteAction;

    return std::nullopt;
}

std::string_view getPaletteKeyActionName(PaletteKeyAction action)
{
    for (auto& entry: paletteKeyActionNames)
        if (entry.action == action)
            return entry.name;

    return "unknown";
}
} // namespace grazer
