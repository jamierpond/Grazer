#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Grazer as its own WebAuthn platform authenticator: the page script hands
// navigator.credentials requests to native as JSON, and these turn them into
// the bytes and replies a relying party verifies. Keys live behind
// PasskeyKeyStore, which each platform implements.
namespace grazer
{
using PasskeyBytes = std::vector<std::uint8_t>;

constexpr auto coseAlgorithmES256 = -7;

struct PasskeyUser
{
    PasskeyBytes id;
    std::string name;
    std::string displayName;
};

struct PasskeyCreationRequest
{
    int requestId = 0;
    PasskeyBytes challenge;
    std::optional<std::string> relyingPartyId;
    std::string relyingPartyName;
    PasskeyUser user;
    std::vector<int> algorithms;
    std::vector<PasskeyBytes> excludedCredentialIds;
    std::string userVerification;
};

struct PasskeyAssertionRequest
{
    int requestId = 0;
    PasskeyBytes challenge;
    std::optional<std::string> relyingPartyId;
    std::vector<PasskeyBytes> allowedCredentialIds;
    std::string userVerification;
};

enum class PasskeyOperation
{
    create,
    get,
    abort,
    unknown
};

struct PasskeyMessageHeader
{
    int requestId = 0;
    PasskeyOperation operation = PasskeyOperation::unknown;
};

PasskeyMessageHeader readPasskeyMessageHeader(const std::string& messageJSON);

// Nullopt when the challenge or the user id is missing or not base64url.
std::optional<PasskeyCreationRequest>
    parsePasskeyCreationRequest(const std::string& messageJSON);

// Nullopt when the challenge is missing or not base64url.
std::optional<PasskeyAssertionRequest>
    parsePasskeyAssertionRequest(const std::string& messageJSON);

bool isRelyingPartyAllowedForOrigin(const std::string& relyingPartyId,
                                    const std::string& origin);

// The request's rp id, or the origin's host when it names none; nullopt when
// the origin may not use it.
std::optional<std::string>
    resolveRelyingPartyId(const std::optional<std::string>& requestedRelyingPartyId,
                          const std::string& origin);

bool isES256Offered(const PasskeyCreationRequest& request);

// The origin as WebAuthn's clientDataJSON writes it: "https://host", with the
// port only when it is not the scheme's default. A port of 0 means the default.
std::string serializeOrigin(const std::string& scheme, const std::string& host, int port);

bool isURLOnOrigin(const std::string& url, const std::string& origin);

enum class PasskeyCeremony
{
    creation,
    assertion
};

std::string buildClientDataJSON(PasskeyCeremony ceremony,
                                const PasskeyBytes& challenge,
                                const std::string& origin);

// A P-256 public key as its two 32-byte big-endian affine coordinates.
struct PasskeyPublicKey
{
    PasskeyBytes x;
    PasskeyBytes y;
};

PasskeyBytes buildCreationAuthenticatorData(const std::string& relyingPartyId,
                                            const PasskeyBytes& credentialId,
                                            const PasskeyPublicKey& publicKey);

PasskeyBytes buildAssertionAuthenticatorData(const std::string& relyingPartyId);

PasskeyBytes buildAttestationObject(const PasskeyBytes& authenticatorData);

PasskeyBytes buildBytesToSignForAssertion(const PasskeyBytes& authenticatorData,
                                          const std::string& clientDataJSON);

struct PasskeyCredential
{
    PasskeyBytes credentialId;
    std::string relyingPartyId;
    PasskeyUser user;
};

struct CreatedPasskey
{
    PasskeyCredential credential;
    PasskeyPublicKey publicKey;
};

// The account as the user knows it: "Jamie (jamie@example.com)".
std::string describePasskeyAccount(const PasskeyUser& user);

// What the Touch ID sheet says Grazer is trying to do, after "Grazer is trying to".
std::string describePasskeyCreationReason(const std::string& relyingPartyId,
                                          const PasskeyUser& user);
std::string describePasskeySigningReason(const PasskeyCredential& credential);

// A credential as the bytes stored on its key's keychain item, and back again;
// nullopt for bytes Grazer did not write.
PasskeyBytes encodePasskeyCredentialMetadata(const PasskeyCredential& credential);
std::optional<PasskeyCredential>
    decodePasskeyCredentialMetadata(const PasskeyBytes& metadata);

class PasskeyKeyStore
{
public:
    virtual ~PasskeyKeyStore() = default;

    // Nullopt when the user declined or the key could not be made.
    virtual std::optional<CreatedPasskey>
        createKey(const std::string& relyingPartyId, const PasskeyUser& user) = 0;

    virtual std::vector<PasskeyCredential>
        listCredentials(const std::string& relyingPartyId) = 0;

    // An ES256 signature: ECDSA P-256 over SHA-256 of `bytesToSign`, DER-encoded.
    // Nullopt when the user declined or the credential is gone.
    virtual std::optional<PasskeyBytes>
        signWithCredential(const PasskeyCredential& credential,
                           const PasskeyBytes& bytesToSign) = 0;
};

enum class PasskeyErrorName
{
    notAllowed,
    invalidState,
    security,
    notSupported
};

struct PasskeyError
{
    PasskeyErrorName name = PasskeyErrorName::notAllowed;
    std::string message;
};

std::string buildPasskeyErrorReply(int requestId, const PasskeyError& error);

// The reply for the page: the new credential, or the error to throw.
std::string answerPasskeyCreationRequest(const PasskeyCreationRequest& request,
                                         const std::string& origin,
                                         PasskeyKeyStore& keyStore);

// The held credentials the request may use, for the user to choose from.
std::variant<std::vector<PasskeyCredential>, PasskeyError>
    findCredentialsForAssertion(const PasskeyAssertionRequest& request,
                                const std::string& origin,
                                PasskeyKeyStore& keyStore);

std::string answerPasskeyAssertionRequest(const PasskeyAssertionRequest& request,
                                          const std::string& origin,
                                          const PasskeyCredential& credential,
                                          PasskeyKeyStore& keyStore);
} // namespace grazer
