#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace grazer
{
// RFC 4648 §5, unpadded: the encoding WebAuthn's JSON forms use for bytes.
std::string encodeBase64Url(const std::vector<std::uint8_t>& bytes);

// Nullopt on text that is not base64url. Trailing padding is accepted.
std::optional<std::vector<std::uint8_t>> decodeBase64Url(std::string_view text);
} // namespace grazer
