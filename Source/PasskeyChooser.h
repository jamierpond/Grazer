#pragma once

#include "Passkey.h"

#include <cstddef>
#include <optional>
#include <vector>

// Asks, in a modal alert, which of one site's passkeys to sign in with. The
// chosen index, or nullopt when the user cancels. Always nullopt on Windows,
// which keeps no passkeys.
std::optional<std::size_t>
    askWhichPasskeyToUse(const std::vector<grazer::PasskeyCredential>& credentials);
