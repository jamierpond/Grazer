#include "Passkey.h"

#include "Base64Url.h"

#include <Miro/Reflect.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace grazer
{
namespace
{
struct WireMessageHeader
{
    int requestId = 0;
    std::string operation;

    MIRO_REFLECT(requestId, operation)
};

struct WireRelyingParty
{
    std::optional<std::string> id;
    std::string name;

    MIRO_REFLECT(id, name)
};

struct WireUser
{
    std::string id;
    std::string name;
    std::string displayName;

    MIRO_REFLECT(id, name, displayName)
};

struct WireCredentialParameter
{
    std::string type;
    int alg = 0;

    MIRO_REFLECT(type, alg)
};

struct WireCredentialDescriptor
{
    std::string type;
    std::string id;

    MIRO_REFLECT(type, id)
};

struct WireAuthenticatorSelection
{
    std::string userVerification;

    MIRO_REFLECT(userVerification)
};

struct WireCreationOptions
{
    std::string challenge;
    WireRelyingParty rp;
    WireUser user;
    std::vector<WireCredentialParameter> pubKeyCredParams;
    std::vector<WireCredentialDescriptor> excludeCredentials;
    WireAuthenticatorSelection authenticatorSelection;

    MIRO_REFLECT(challenge,
                 rp,
                 user,
                 pubKeyCredParams,
                 excludeCredentials,
                 authenticatorSelection)
};

struct WireCreationMessage
{
    int requestId = 0;
    WireCreationOptions publicKey;

    MIRO_REFLECT(requestId, publicKey)
};

struct WireAssertionOptions
{
    std::string challenge;
    std::optional<std::string> rpId;
    std::vector<WireCredentialDescriptor> allowCredentials;
    std::string userVerification;

    MIRO_REFLECT(challenge, rpId, allowCredentials, userVerification)
};

struct WireAssertionMessage
{
    int requestId = 0;
    WireAssertionOptions publicKey;

    MIRO_REFLECT(requestId, publicKey)
};

struct OperationName
{
    std::string_view name;
    PasskeyOperation operation;
};

constexpr auto operationNames = std::array {
    OperationName {"create", PasskeyOperation::create},
    OperationName {"get", PasskeyOperation::get},
    OperationName {"abort", PasskeyOperation::abort},
};

constexpr auto schemeSeparator = std::string_view {"://"};
constexpr auto secureScheme = std::string_view {"https"};
constexpr auto plainScheme = std::string_view {"http"};
constexpr auto plainSchemeHostAllowed = std::string_view {"localhost"};

std::vector<PasskeyBytes>
    decodeCredentialIds(const std::vector<WireCredentialDescriptor>& descriptors)
{
    auto ids = std::vector<PasskeyBytes> {};

    for (auto& descriptor: descriptors)
    {
        if (auto id = decodeBase64Url(descriptor.id))
            ids.push_back(*id);
    }

    return ids;
}

std::optional<PasskeyBytes> decodeNonEmptyBase64Url(const std::string& text)
{
    auto decoded = decodeBase64Url(text);

    if (!decoded || decoded->empty())
        return std::nullopt;

    return decoded;
}

struct ParsedOrigin
{
    std::string scheme;
    std::string host;
};

std::optional<ParsedOrigin> parseOrigin(const std::string& origin)
{
    auto separator = origin.find(schemeSeparator);

    if (separator == std::string::npos)
        return std::nullopt;

    auto hostStart = separator + schemeSeparator.size();
    auto hostEnd = origin.find_first_of(":/", hostStart);
    auto host = origin.substr(hostStart, hostEnd - hostStart);

    if (host.empty())
        return std::nullopt;

    return ParsedOrigin {origin.substr(0, separator), host};
}

bool isSecureOrigin(const ParsedOrigin& origin)
{
    if (origin.scheme == secureScheme)
        return true;

    return origin.scheme == plainScheme && origin.host == plainSchemeHostAllowed;
}

std::optional<ParsedOrigin> parseSecureOrigin(const std::string& origin)
{
    auto parsed = parseOrigin(origin);

    if (!parsed || !isSecureOrigin(*parsed))
        return std::nullopt;

    return parsed;
}

bool isIPAddressHost(const std::string& host)
{
    auto lastLabel = host.substr(host.find_last_of('.') + 1);
    auto isDigit = [](char character)
    { return character >= '0' && character <= '9'; };

    return host.starts_with('[') || std::ranges::all_of(lastLabel, isDigit);
}

// Deliberately not a public-suffix-list check: a page may name "co.uk" as its rp id.
bool isRegistrableSuffixOfHost(const std::string& relyingPartyId,
                               const std::string& host)
{
    if (relyingPartyId.find('.') == std::string::npos || isIPAddressHost(host))
        return false;

    return host.ends_with("." + relyingPartyId);
}
} // namespace

PasskeyMessageHeader readPasskeyMessageHeader(const std::string& messageJSON)
{
    auto wire = Miro::createFromJSONString<WireMessageHeader>(messageJSON);
    auto header = PasskeyMessageHeader {wire.requestId, PasskeyOperation::unknown};

    for (auto& entry: operationNames)
    {
        if (entry.name == wire.operation)
            header.operation = entry.operation;
    }

    return header;
}

std::optional<PasskeyCreationRequest>
    parsePasskeyCreationRequest(const std::string& messageJSON)
{
    auto wire = Miro::createFromJSONString<WireCreationMessage>(messageJSON);
    auto& options = wire.publicKey;
    auto challenge = decodeNonEmptyBase64Url(options.challenge);
    auto userId = decodeNonEmptyBase64Url(options.user.id);

    if (!challenge || !userId)
        return std::nullopt;

    auto request = PasskeyCreationRequest {};
    request.requestId = wire.requestId;
    request.challenge = *challenge;
    request.relyingPartyId = options.rp.id;
    request.relyingPartyName = options.rp.name;
    request.user = {*userId, options.user.name, options.user.displayName};
    request.excludedCredentialIds = decodeCredentialIds(options.excludeCredentials);
    request.userVerification = options.authenticatorSelection.userVerification;

    for (auto& parameter: options.pubKeyCredParams)
        request.algorithms.push_back(parameter.alg);

    return request;
}

std::optional<PasskeyAssertionRequest>
    parsePasskeyAssertionRequest(const std::string& messageJSON)
{
    auto wire = Miro::createFromJSONString<WireAssertionMessage>(messageJSON);
    auto& options = wire.publicKey;
    auto challenge = decodeNonEmptyBase64Url(options.challenge);

    if (!challenge)
        return std::nullopt;

    auto request = PasskeyAssertionRequest {};
    request.requestId = wire.requestId;
    request.challenge = *challenge;
    request.relyingPartyId = options.rpId;
    request.allowedCredentialIds = decodeCredentialIds(options.allowCredentials);
    request.userVerification = options.userVerification;
    return request;
}

bool isRelyingPartyAllowedForOrigin(const std::string& relyingPartyId,
                                    const std::string& origin)
{
    auto parsed = parseSecureOrigin(origin);

    if (!parsed || relyingPartyId.empty())
        return false;

    if (relyingPartyId == parsed->host)
        return true;

    return isRegistrableSuffixOfHost(relyingPartyId, parsed->host);
}

std::optional<std::string>
    resolveRelyingPartyId(const std::optional<std::string>& requestedRelyingPartyId,
                          const std::string& origin)
{
    auto parsed = parseOrigin(origin);

    if (!parsed)
        return std::nullopt;

    auto relyingPartyId = requestedRelyingPartyId.value_or(parsed->host);

    if (!isRelyingPartyAllowedForOrigin(relyingPartyId, origin))
        return std::nullopt;

    return relyingPartyId;
}

bool isES256Offered(const PasskeyCreationRequest& request)
{
    // WebAuthn §5.1.3: an empty pubKeyCredParams means ES256 and RS256.
    if (request.algorithms.empty())
        return true;

    auto found = std::ranges::find(request.algorithms, coseAlgorithmES256);
    return found != request.algorithms.end();
}
} // namespace grazer
