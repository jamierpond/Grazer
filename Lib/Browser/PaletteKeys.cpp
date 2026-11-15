#include "PaletteKeys.h"

#include <algorithm>
#include <array>

namespace grazer
{
namespace
{
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
std::string getSearchKeyCharacters(const PressedKeyText& key)
{
    if (key.command)
        return {};

    if (key.control)
        return key.charactersIgnoringModifiers;

    if (hasControlCharacter(key.characters) || isFunctionKeyCharacter(key.characters))
        return {};

    return key.characters;
}
} // namespace grazer
