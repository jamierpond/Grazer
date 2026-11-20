#include "Base64Url.h"

#include <eacp/Core/Utils/Base64.h>

#include <algorithm>

namespace grazer
{
namespace
{
constexpr auto base64QuantumLength = std::size_t {4};
constexpr auto paddingCharacter = '=';

void replaceCharacter(std::string& text, char from, char to)
{
    std::replace(text.begin(), text.end(), from, to);
}
} // namespace

std::string encodeBase64Url(const std::vector<std::uint8_t>& bytes)
{
    auto* firstCharacter = reinterpret_cast<const char*>(bytes.data());
    auto text = eacp::Base64::encode(std::string_view(firstCharacter, bytes.size()));

    replaceCharacter(text, '+', '-');
    replaceCharacter(text, '/', '_');
    text.erase(text.find_last_not_of(paddingCharacter) + 1);
    return text;
}

std::optional<std::vector<std::uint8_t>> decodeBase64Url(std::string_view text)
{
    auto padded = std::string(text);
    padded.erase(padded.find_last_not_of(paddingCharacter) + 1);

    if (padded.find_first_of("+/=") != std::string::npos)
        return std::nullopt;

    replaceCharacter(padded, '-', '+');
    replaceCharacter(padded, '_', '/');

    auto remainder = padded.size() % base64QuantumLength;

    if (remainder != 0)
        padded.append(base64QuantumLength - remainder, paddingCharacter);

    auto decoded = eacp::Base64::decode(padded);

    if (!decoded)
        return std::nullopt;

    return std::vector<std::uint8_t>(decoded->begin(), decoded->end());
}
} // namespace grazer
