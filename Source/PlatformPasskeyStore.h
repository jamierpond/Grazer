#pragma once

#include "Passkey.h"

#include <memory>

// Where Grazer keeps its passkeys: the Secure Enclave on macOS, nowhere yet on
// Windows.
namespace PlatformPasskeyStore
{

// True when this process can keep passkeys: it is signed with an application
// identifier and a keychain access group, and the Mac has a Secure Enclave.
// False for an ad-hoc build and on Windows.
bool isAvailable();

// The key store one page request uses. Creating and signing block while the
// user is asked, so they run off the main thread. Cancelling, from any thread,
// closes the request's Touch ID sheet and fails its calls from then on.
class RequestKeyStore : public grazer::PasskeyKeyStore
{
public:
    virtual void cancelUserPrompt() = 0;
};

std::shared_ptr<RequestKeyStore> createRequestKeyStore();

} // namespace PlatformPasskeyStore
