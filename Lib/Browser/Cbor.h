#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace grazer
{
// The slice of CBOR (RFC 8949) a WebAuthn attestation object needs: integers,
// byte and text strings, and maps whose entries the caller writes in order.
class CborWriter
{
public:
    void writeUnsignedInteger(std::uint64_t value);
    void writeSignedInteger(std::int64_t value);
    void writeByteString(std::span<const std::uint8_t> bytes);
    void writeTextString(std::string_view text);
    void beginMap(std::size_t entryCount);

    const std::vector<std::uint8_t>& getBytes() const;

private:
    void writeHead(std::uint8_t majorType, std::uint64_t argument);
    void writeBigEndian(std::uint64_t value, std::size_t byteCount);

    std::vector<std::uint8_t> bytes;
};
} // namespace grazer
