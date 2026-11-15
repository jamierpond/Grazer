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

auto tEscapeWithNoTextCancels = test("PaletteKeys/escape with no text cancels the palette") = []
{
    check(getOutcomeOfKeys("", {escapeKey}) == PaletteKeyAction::cancel);
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

namespace
{
searchkeys::SearchBox makeBoxInNormalMode(const std::string& text)
{
    auto box = searchkeys::SearchBox {};
    box.setTextAndCaret(text, (int) text.size());
    box.handleKey(escapeKey);
    return box;
}
} // namespace

auto tEscapePutsCursorOnLastCharacter = test("PaletteKeys/escape puts the block cursor on the last character") = []
{
    auto text = std::string {"hello world"};
    auto lastCharacter = (int) text.size() - 1;
    check(makeBoxInNormalMode(text).getCaret() == lastCharacter);
};

auto tBackWordMovesToWordStart = test("PaletteKeys/b from the end of hello world lands on world") = []
{
    auto box = makeBoxInNormalMode("hello world");
    box.handleKey(makeTypedKey("b"));
    auto startOfWorld = 6;
    check(box.getCaret() == startOfWorld);
};

auto tXDeletesCharacter = test("PaletteKeys/x in normal mode deletes the character under the cursor") = []
{
    auto box = makeBoxInNormalMode("git");
    box.handleKey(makeTypedKey("x"));
    check(box.getText() == "gi");
};

auto tJMovesHighlight = test("PaletteKeys/j in normal mode moves the highlight down") = []
{
    check(getOutcomeOfKeys("git", {escapeKey, makeTypedKey("j")}) == PaletteKeyAction::moveDown);
};

auto tControlWDeletesWord = test("PaletteKeys/ctrl+w in insert mode deletes the word behind the caret") = []
{
    auto controlW = searchkeys::Key {};
    controlW.characters = getSearchKeyCharacters(
        {.characters = "\x17", .charactersIgnoringModifiers = "w", .control = true});
    controlW.control = true;

    auto box = searchkeys::SearchBox {};
    box.setTextAndCaret("hello world", 11);
    check(box.handleKey(controlW) == searchkeys::Action::none);
    check(box.getText() == "hello ");
};

auto tCommandChordTypesNothing = test("PaletteKeys/a command chord types nothing into the search box") = []
{
    check(getSearchKeyCharacters({.characters = "c", .charactersIgnoringModifiers = "c", .command = true})
              .empty());
};
