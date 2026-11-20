#include "Cbor.h"

#include <array>
#include <limits>

namespace grazer
{
namespace
{
constexpr auto majorTypeUnsignedInteger = std::uint8_t {0};
constexpr auto majorTypeNegativeInteger = std::uint8_t {1};
constexpr auto majorTypeByteString = std::uint8_t {2};
constexpr auto majorTypeTextString = std::uint8_t {3};
constexpr auto majorTypeMap = std::uint8_t {5};

constexpr auto majorTypeShift = 5;
constexpr auto bitsPerByte = 8;
constexpr auto lowestByteMask = std::uint64_t {0xff};

constexpr auto largestInlineArgument = std::uint64_t {23};

struct ArgumentWidth
{
    std::uint64_t largestValue;
    std::uint8_t additionalInformation;
    std::size_t byteCount;
};

// RFC 8949 §3: additional information 24..27 says the argument follows in
// 1, 2, 4 or 8 bytes.
constexpr auto argumentWidths = std::array {
    ArgumentWidth {std::numeric_limits<std::uint8_t>::max(), 24, 1},
    ArgumentWidth {std::numeric_limits<std::uint16_t>::max(), 25, 2},
    ArgumentWidth {std::numeric_limits<std::uint32_t>::max(), 26, 4},
    ArgumentWidth {std::numeric_limits<std::uint64_t>::max(), 27, 8},
};
} // namespace

void CborWriter::writeUnsignedInteger(std::uint64_t value)
{
    writeHead(majorTypeUnsignedInteger, value);
}

void CborWriter::writeSignedInteger(std::int64_t value)
{
    if (value >= 0)
    {
        auto unsignedValue = static_cast<std::uint64_t>(value);
        writeHead(majorTypeUnsignedInteger, unsignedValue);
        return;
    }

    // A negative integer n is encoded as -1 - n.
    auto encodedValue = static_cast<std::uint64_t>(-(value + 1));
    writeHead(majorTypeNegativeInteger, encodedValue);
}

void CborWriter::writeByteString(std::span<const std::uint8_t> bytesToWrite)
{
    writeHead(majorTypeByteString, bytesToWrite.size());
    bytes.insert(bytes.end(), bytesToWrite.begin(), bytesToWrite.end());
}

void CborWriter::writeTextString(std::string_view text)
{
    writeHead(majorTypeTextString, text.size());
    bytes.insert(bytes.end(), text.begin(), text.end());
}

void CborWriter::beginMap(std::size_t entryCount)
{
    writeHead(majorTypeMap, entryCount);
}

const std::vector<std::uint8_t>& CborWriter::getBytes() const
{
    return bytes;
}

void CborWriter::writeHead(std::uint8_t majorType, std::uint64_t argument)
{
    auto majorTypeBits = static_cast<std::uint8_t>(majorType << majorTypeShift);

    if (argument <= largestInlineArgument)
    {
        auto inlineArgument = static_cast<std::uint8_t>(argument);
        bytes.push_back(majorTypeBits | inlineArgument);
        return;
    }

    for (auto& width: argumentWidths)
    {
        if (argument <= width.largestValue)
        {
            bytes.push_back(majorTypeBits | width.additionalInformation);
            writeBigEndian(argument, width.byteCount);
            return;
        }
    }
}

void CborWriter::writeBigEndian(std::uint64_t value, std::size_t byteCount)
{
    for (auto index = byteCount; index > 0; --index)
    {
        auto shift = (index - 1) * bitsPerByte;
        auto byte = static_cast<std::uint8_t>((value >> shift) & lowestByteMask);
        bytes.push_back(byte);
    }
}
} // namespace grazer
