#include "Passkey.h"

#include "Base64Url.h"

#include <Miro/Json.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace grazer
{
namespace
{
struct ErrorDomExceptionName
{
    PasskeyErrorName name;
    std::string_view domExceptionName;
};

constexpr auto errorDomExceptionNames = std::array {
    ErrorDomExceptionName {PasskeyErrorName::notAllowed, "NotAllowedError"},
    ErrorDomExceptionName {PasskeyErrorName::invalidState, "InvalidStateError"},
    ErrorDomExceptionName {PasskeyErrorName::security, "SecurityError"},
    ErrorDomExceptionName {PasskeyErrorName::notSupported, "NotSupportedError"},
};

constexpr auto internalTransport = "internal";

// DER SubjectPublicKeyInfo header for an id-ecPublicKey on prime256v1, followed
// by the uncompressed point marker; x and y come after it.
constexpr auto p256SubjectPublicKeyInfoPrefix = std::to_array<std::uint8_t>(
    {0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48,
     0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48,
     0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04});

std::string getDomExceptionName(PasskeyErrorName name)
{
    for (auto& entry: errorDomExceptionNames)
    {
        if (entry.name == name)
            return std::string(entry.domExceptionName);
    }

    return "NotAllowedError";
}

PasskeyBytes buildSubjectPublicKeyInfo(const PasskeyPublicKey& publicKey)
{
    auto bytes = PasskeyBytes(p256SubjectPublicKeyInfoPrefix.begin(),
                              p256SubjectPublicKeyInfoPrefix.end());
    bytes.insert(bytes.end(), publicKey.x.begin(), publicKey.x.end());
    bytes.insert(bytes.end(), publicKey.y.begin(), publicKey.y.end());
    return bytes;
}

std::string buildReply(int requestId,
                       const std::string& outcomeName,
                       Miro::Json::Object outcome)
{
    auto reply = Miro::Json::Object {
        {"requestId", requestId},
        {outcomeName, std::move(outcome)},
    };

    return Miro::Json::print(reply);
}

std::string encodeTextAsBase64Url(const std::string& text)
{
    return encodeBase64Url(PasskeyBytes(text.begin(), text.end()));
}

std::string buildPasskeyCreationReply(const PasskeyCreationRequest& request,
                                      const std::string& clientDataJSON,
                                      const PasskeyBytes& authenticatorData,
                                      const CreatedPasskey& passkey)
{
    auto attestationObject = buildAttestationObject(authenticatorData);
    auto subjectPublicKeyInfo = buildSubjectPublicKeyInfo(passkey.publicKey);

    auto response = Miro::Json::Object {
        {"clientDataJSON", encodeTextAsBase64Url(clientDataJSON)},
        {"attestationObject", encodeBase64Url(attestationObject)},
        {"authenticatorData", encodeBase64Url(authenticatorData)},
        {"publicKey", encodeBase64Url(subjectPublicKeyInfo)},
        {"publicKeyAlgorithm", coseAlgorithmES256},
        {"transports", Miro::Json::Array {internalTransport}},
    };

    auto credential = Miro::Json::Object {
        {"id", encodeBase64Url(passkey.credential.credentialId)},
        {"response", std::move(response)},
    };

    return buildReply(request.requestId, "credential", std::move(credential));
}

struct SignedAssertion
{
    std::string clientDataJSON;
    PasskeyBytes authenticatorData;
    PasskeyBytes signature;
};

std::string buildPasskeyAssertionReply(const PasskeyAssertionRequest& request,
                                       const PasskeyCredential& credential,
                                       const SignedAssertion& assertion)
{
    auto response = Miro::Json::Object {
        {"clientDataJSON", encodeTextAsBase64Url(assertion.clientDataJSON)},
        {"authenticatorData", encodeBase64Url(assertion.authenticatorData)},
        {"signature", encodeBase64Url(assertion.signature)},
        {"userHandle", encodeBase64Url(credential.user.id)},
    };

    auto credentialObject = Miro::Json::Object {
        {"id", encodeBase64Url(credential.credentialId)},
        {"response", std::move(response)},
    };

    return buildReply(request.requestId, "credential", std::move(credentialObject));
}

bool isAnyCredentialHeld(const std::vector<PasskeyBytes>& credentialIds,
                         const std::vector<PasskeyCredential>& heldCredentials)
{
    auto isHeld = [&](const PasskeyBytes& id)
    {
        auto hasId = [&](const PasskeyCredential& held)
        { return held.credentialId == id; };
        return std::ranges::any_of(heldCredentials, hasId);
    };

    return std::ranges::any_of(credentialIds, isHeld);
}

std::vector<PasskeyCredential>
    keepAllowedCredentials(std::vector<PasskeyCredential> credentials,
                           const std::vector<PasskeyBytes>& allowedCredentialIds)
{
    if (allowedCredentialIds.empty())
        return credentials;

    auto isNotAllowed = [&](const PasskeyCredential& credential)
    {
        auto found =
            std::ranges::find(allowedCredentialIds, credential.credentialId);
        return found == allowedCredentialIds.end();
    };

    std::erase_if(credentials, isNotAllowed);
    return credentials;
}

PasskeyError makeSecurityError()
{
    return {PasskeyErrorName::security,
            "This page's origin may not use the requested relying party id."};
}

PasskeyError makeDeclinedError()
{
    return {PasskeyErrorName::notAllowed, "The request was declined or timed out."};
}
} // namespace

std::string buildPasskeyErrorReply(int requestId, const PasskeyError& error)
{
    auto errorObject = Miro::Json::Object {
        {"name", getDomExceptionName(error.name)},
        {"message", error.message},
    };

    return buildReply(requestId, "error", errorObject);
}

std::string answerPasskeyCreationRequest(const PasskeyCreationRequest& request,
                                         const std::string& origin,
                                         PasskeyKeyStore& keyStore)
{
    auto relyingPartyId = resolveRelyingPartyId(request.relyingPartyId, origin);

    if (!relyingPartyId)
        return buildPasskeyErrorReply(request.requestId, makeSecurityError());

    if (!isES256Offered(request))
    {
        auto error = PasskeyError {PasskeyErrorName::notSupported,
                                   "Grazer makes only ES256 (P-256) passkeys."};
        return buildPasskeyErrorReply(request.requestId, error);
    }

    auto held = keyStore.listCredentials(*relyingPartyId);

    if (isAnyCredentialHeld(request.excludedCredentialIds, held))
    {
        auto error =
            PasskeyError {PasskeyErrorName::invalidState,
                          "Grazer already holds a passkey for this account."};
        return buildPasskeyErrorReply(request.requestId, error);
    }

    auto passkey = keyStore.createKey(*relyingPartyId, request.user);

    if (!passkey)
        return buildPasskeyErrorReply(request.requestId, makeDeclinedError());

    auto clientDataJSON =
        buildClientDataJSON(PasskeyCeremony::creation, request.challenge, origin);
    auto authenticatorData = buildCreationAuthenticatorData(
        *relyingPartyId, passkey->credential.credentialId, passkey->publicKey);

    return buildPasskeyCreationReply(
        request, clientDataJSON, authenticatorData, *passkey);
}

std::variant<std::vector<PasskeyCredential>, PasskeyError>
    findCredentialsForAssertion(const PasskeyAssertionRequest& request,
                                const std::string& origin,
                                PasskeyKeyStore& keyStore)
{
    auto relyingPartyId = resolveRelyingPartyId(request.relyingPartyId, origin);

    if (!relyingPartyId)
        return makeSecurityError();

    auto credentials = keepAllowedCredentials(
        keyStore.listCredentials(*relyingPartyId), request.allowedCredentialIds);

    if (credentials.empty())
    {
        return PasskeyError {PasskeyErrorName::notAllowed,
                             "Grazer holds no passkey for this site."};
    }

    return credentials;
}

std::string answerPasskeyAssertionRequest(const PasskeyAssertionRequest& request,
                                          const std::string& origin,
                                          const PasskeyCredential& credential,
                                          PasskeyKeyStore& keyStore)
{
    auto relyingPartyId = resolveRelyingPartyId(request.relyingPartyId, origin);

    if (!relyingPartyId)
        return buildPasskeyErrorReply(request.requestId, makeSecurityError());

    auto clientDataJSON =
        buildClientDataJSON(PasskeyCeremony::assertion, request.challenge, origin);
    auto authenticatorData = buildAssertionAuthenticatorData(*relyingPartyId);
    auto bytesToSign =
        buildBytesToSignForAssertion(authenticatorData, clientDataJSON);
    auto signature =
        keyStore.signWithCredential(credential.credentialId, bytesToSign);

    if (!signature)
        return buildPasskeyErrorReply(request.requestId, makeDeclinedError());

    auto assertion = SignedAssertion {clientDataJSON, authenticatorData, *signature};
    return buildPasskeyAssertionReply(request, credential, assertion);
}
} // namespace grazer
