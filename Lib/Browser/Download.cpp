#include "Download.h"

#include <algorithm>
#include <cctype>

namespace grazer
{
namespace
{
constexpr auto attachmentDispositionType = std::string_view {"attachment"};

bool isHeaderWhitespace(char character)
{
    return character == ' ' || character == '\t';
}

std::string_view getDispositionType(std::string_view header)
{
    auto type = header.substr(0, header.find(';'));

    while (!type.empty() && isHeaderWhitespace(type.front()))
        type.remove_prefix(1);

    while (!type.empty() && isHeaderWhitespace(type.back()))
        type.remove_suffix(1);

    return type;
}

bool isSameIgnoringCase(char left, char right)
{
    auto leftByte = static_cast<unsigned char>(left);
    auto rightByte = static_cast<unsigned char>(right);
    return std::tolower(leftByte) == std::tolower(rightByte);
}
} // namespace

bool isAttachmentDisposition(std::string_view header)
{
    return std::ranges::equal(
        getDispositionType(header), attachmentDispositionType, isSameIgnoringCase);
}
} // namespace grazer
