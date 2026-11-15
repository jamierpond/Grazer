#include <PaletteKeys.h>

#include <NanoTest/NanoTest.h>

#include <string>
#include <vector>

using namespace nano;
using namespace grazer;

namespace
{
searchkeys::Key makeTypedKey(const std::string& characters)
{
    auto key = searchkeys::Key {};
    key.characters = characters;
    return key;
}

searchkeys::Key makeSpecialKey(searchkeys::SpecialKey special)
{
    auto key = searchkeys::Key {};
    key.special = special;
    return key;
}

const auto escapeKey = makeSpecialKey(searchkeys::SpecialKey::escape);

// Runs `keys` in a freshly opened palette that has `text` typed, and returns
// the palette outcome of the last key.
std::optional<PaletteKeyAction> getOutcomeOfKeys(const std::string& text,
                                                 const std::vector<searchkeys::Key>& keys)
{
    auto box = searchkeys::SearchBox {};
    box.setTextAndCaret(text, (int) text.size());
    auto action = searchkeys::Action::none;

    for (auto& key: keys)
        action = box.handleKey(key);

    return getPaletteKeyAction(action);
}
} // namespace

auto tTypingEditsText = test("PaletteKeys/typing in insert mode edits the query, no palette outcome") = []
{
    check(!getOutcomeOfKeys("", {makeTypedKey("j")}).has_value());
};

auto tEscapeInInsertStaysOpen = test("PaletteKeys/escape in insert mode keeps the palette open") = []
{
    check(!getOutcomeOfKeys("git", {escapeKey}).has_value());
};

auto tEscapeInNormalCancels = test("PaletteKeys/escape in normal mode cancels the palette") = []
{
    check(getOutcomeOfKeys("git", {escapeKey, escapeKey}) == PaletteKeyAction::cancel);
};

auto tNormalKeys = test("PaletteKeys/normal mode list keys reach the palette outcomes") = []
{
    auto expectations = std::vector<std::pair<std::vector<searchkeys::Key>, PaletteKeyAction>> {
        {{makeTypedKey("j")}, PaletteKeyAction::moveDown},
        {{makeTypedKey("k")}, PaletteKeyAction::moveUp},
        {{makeTypedKey("g"), makeTypedKey("g")}, PaletteKeyAction::moveToFirst},
        {{makeTypedKey("G")}, PaletteKeyAction::moveToLast},
        {{makeSpecialKey(searchkeys::SpecialKey::enter)}, PaletteKeyAction::activate},
        {{makeTypedKey("s")}, PaletteKeyAction::toggleStar},
    };

    for (auto& [keys, expected]: expectations)
    {
        auto withEscapeFirst = std::vector<searchkeys::Key> {escapeKey};
        withEscapeFirst.insert(withEscapeFirst.end(), keys.begin(), keys.end());
        check(getOutcomeOfKeys("git", withEscapeFirst) == expected);
    }
};

auto tTextEditsHaveNoOutcome = test("PaletteKeys/x, dd and b edit the query instead of closing a row") = []
{
    check(!getOutcomeOfKeys("git", {escapeKey, makeTypedKey("x")}).has_value());
    check(!getOutcomeOfKeys("git", {escapeKey, makeTypedKey("d"), makeTypedKey("d")}).has_value());
    check(!getOutcomeOfKeys("git", {escapeKey, makeTypedKey("b")}).has_value());
};

auto tCommandEnterOpensInNewTab = test("PaletteKeys/command enter in normal mode opens in a new tab") = []
{
    auto commandEnter = makeSpecialKey(searchkeys::SpecialKey::enter);
    commandEnter.command = true;
    check(getOutcomeOfKeys("git", {escapeKey, commandEnter}) == PaletteKeyAction::activateInNewTab);
};
