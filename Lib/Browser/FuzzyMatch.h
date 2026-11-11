#pragma once

#include <algorithm>
#include <cctype>
#include <climits>
#include <optional>
#include <string>
#include <vector>

namespace grazer
{
namespace fuzzy
{
constexpr auto letterScore = 1;
constexpr auto consecutiveLetterScore = 8;
constexpr auto boundaryBonus = 4;
constexpr auto maximumGapPenalty = 3;
constexpr auto unreachable = INT_MIN / 2;

inline char toLowerCharacter(char character)
{
    auto unsignedCharacter = (unsigned char) character;
    return (char) std::tolower(unsignedCharacter);
}

inline bool isWordBoundary(const std::string& text, std::size_t index)
{
    if (index == 0)
        return true;

    auto previous = (unsigned char) text[index - 1];
    return !std::isalnum(previous);
}

inline int calculateGapPenalty(std::size_t skippedCharacters)
{
    auto cappedSkip = std::min<std::size_t>(skippedCharacters, maximumGapPenalty);
    return (int) cappedSkip;
}

inline std::string removeSpaces(const std::string& query)
{
    auto letters = std::string {};

    for (auto character: query)
        if (character != ' ')
            letters.push_back(toLowerCharacter(character));

    return letters;
}

// Best score of matching the first letter at each text position.
inline std::vector<int> scoreFirstLetter(char letter, const std::string& text)
{
    auto scores = std::vector<int>(text.size(), unreachable);

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (toLowerCharacter(text[index]) != letter)
            continue;

        auto boundary = isWordBoundary(text, index) ? boundaryBonus : 0;
        scores[index] = letterScore + boundary - calculateGapPenalty(index);
    }

    return scores;
}

// Best score of the previous letter ending before `index`, with the bonus or
// gap penalty of moving from there to `index`.
inline int findBestPreviousScore(const std::vector<int>& previousScores,
                                 std::size_t index,
                                 int bestFarPrevious)
{
    auto best = bestFarPrevious - maximumGapPenalty;
    auto adjacent = previousScores[index - 1];

    if (adjacent > unreachable)
        best = std::max(best, adjacent + consecutiveLetterScore - letterScore);

    for (std::size_t skipped = 1; skipped < maximumGapPenalty && skipped < index;
         ++skipped)
        best = std::max(best,
                        previousScores[index - 1 - skipped] - calculateGapPenalty(skipped));

    return best;
}

// Best score of matching the next letter at each text position, given the
// best scores of the previous letter ending at each earlier position.
inline std::vector<int> scoreNextLetter(char letter,
                                        const std::string& text,
                                        const std::vector<int>& previousScores)
{
    auto scores = std::vector<int>(text.size(), unreachable);
    auto bestFarPrevious = unreachable;

    for (std::size_t index = 1; index < text.size(); ++index)
    {
        if (index > maximumGapPenalty)
            bestFarPrevious =
                std::max(bestFarPrevious, previousScores[index - maximumGapPenalty - 1]);

        if (toLowerCharacter(text[index]) != letter)
            continue;

        auto best = findBestPreviousScore(previousScores, index, bestFarPrevious);

        if (best <= unreachable / 2)
            continue;

        auto boundary = isWordBoundary(text, index) ? boundaryBonus : 0;
        scores[index] = best + letterScore + boundary;
    }

    return scores;
}
} // namespace fuzzy

// fzf-style case-insensitive subsequence match that finds the best-scoring
// alignment, not the first one. Consecutive matches and word-boundary hits
// score higher; skipped text costs a little. Spaces in the query are ignored
// so "wiki main" can match "Wikipedia — Main Page".
inline std::optional<int> fuzzyScore(const std::string& query,
                                     const std::string& text)
{
    auto letters = fuzzy::removeSpaces(query);

    if (letters.empty())
        return 0;

    auto scores = fuzzy::scoreFirstLetter(letters.front(), text);

    for (std::size_t index = 1; index < letters.size(); ++index)
        scores = fuzzy::scoreNextLetter(letters[index], text, scores);

    auto best = std::max_element(scores.begin(), scores.end());

    if (best == scores.end() || *best <= fuzzy::unreachable / 2)
        return std::nullopt;

    return *best;
}
} // namespace grazer
