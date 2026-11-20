#include "Passkey.h"

#include "Base64Url.h"

#include <Miro/Reflect.h>

namespace grazer
{
namespace
{
constexpr auto credentialMetadataFormat = 1;
constexpr auto unnamedAccount = "an account with no name";

struct WireCredentialMetadata
{
    int grazerPasskeyFormat = 0;
    std::string credentialId;
    std::string relyingPartyId;
    std::string userId;
    std::string userName;
    std::string userDisplayName;

    MIRO_REFLECT(grazerPasskeyFormat,
                 credentialId,
                 relyingPartyId,
                 userId,
                 userName,
                 userDisplayName)
};

bool isGrazerCredentialMetadata(const WireCredentialMetadata& wire)
{
    if (wire.grazerPasskeyFormat != credentialMetadataFormat)
        return false;

    return !wire.relyingPartyId.empty() && !wire.credentialId.empty();
}
} // namespace

std::string describePasskeyAccount(const PasskeyUser& user)
{
    if (user.name.empty() && user.displayName.empty())
        return unnamedAccount;

    if (user.displayName.empty() || user.displayName == user.name)
        return user.name;

    if (user.name.empty())
        return user.displayName;

    return user.displayName + " (" + user.name + ")";
}

std::string describePasskeyCreationReason(const std::string& relyingPartyId,
                                          const PasskeyUser& user)
{
    return "save a passkey for " + describePasskeyAccount(user) + " on "
           + relyingPartyId;
}

std::string describePasskeySigningReason(const PasskeyCredential& credential)
{
    return "sign in to " + credential.relyingPartyId + " as "
           + describePasskeyAccount(credential.user);
}

PasskeyBytes encodePasskeyCredentialMetadata(const PasskeyCredential& credential)
{
    auto wire = WireCredentialMetadata {credentialMetadataFormat,
                                        encodeBase64Url(credential.credentialId),
                                        credential.relyingPartyId,
                                        encodeBase64Url(credential.user.id),
                                        credential.user.name,
                                        credential.user.displayName};
    auto json = Miro::toJSONString(wire);
    return {json.begin(), json.end()};
}

std::optional<PasskeyCredential>
    decodePasskeyCredentialMetadata(const PasskeyBytes& metadata)
{
    auto json = std::string(metadata.begin(), metadata.end());
    auto wire = Miro::createFromJSONString<WireCredentialMetadata>(json);

    if (!isGrazerCredentialMetadata(wire))
        return std::nullopt;

    auto credentialId = decodeBase64Url(wire.credentialId);
    auto userId = decodeBase64Url(wire.userId);

    if (!credentialId || !userId)
        return std::nullopt;

    auto user = PasskeyUser {*userId, wire.userName, wire.userDisplayName};
    return PasskeyCredential {*credentialId, wire.relyingPartyId, user};
}
} // namespace grazer
