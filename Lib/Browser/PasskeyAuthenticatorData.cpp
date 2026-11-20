#include "Passkey.h"

#include "Base64Url.h"
#include "Cbor.h"

#include <Miro/Json.h>
#include <eacp/Core/Utils/Sha256.h>

#include <array>

namespace grazer
{
namespace
{
constexpr auto flagUserPresent = std::uint8_t {0x01};
constexpr auto flagUserVerified = std::uint8_t {0x04};
constexpr auto flagAttestedCredentialData = std::uint8_t {0x40};

constexpr auto signatureCounterLength = std::size_t {4};
constexpr auto aaguidLength = std::size_t {16};
constexpr auto credentialIdLengthHighByteShift = 8;
constexpr auto lowestByteMask = std::size_t {0xff};

constexpr auto coseKeyLabelKeyType = 1;
constexpr auto coseKeyLabelAlgorithm = 3;
constexpr auto coseKeyLabelCurve = -1;
constexpr auto coseKeyLabelX = -2;
constexpr auto coseKeyLabelY = -3;
constexpr auto coseKeyTypeEC2 = 2;
constexpr auto coseCurveP256 = 1;
constexpr auto coseEC2KeyEntryCount = 5;

constexpr auto attestationObjectEntryCount = 3;
constexpr auto noneAttestationFormat = "none";

PasskeyBytes computeSha256OfText(std::string_view text)
{
    auto hash = eacp::Sha256 {};
    hash.update(text);
    auto digest = hash.finish();
    return {digest.begin(), digest.end()};
}

void append(PasskeyBytes& bytes, const PasskeyBytes& more)
{
    bytes.insert(bytes.end(), more.begin(), more.end());
}

PasskeyBytes buildAuthenticatorDataHeader(const std::string& relyingPartyId,
                                          std::uint8_t flags)
{
    auto bytes = computeSha256OfText(relyingPartyId);
    bytes.push_back(flags);
    bytes.insert(bytes.end(), signatureCounterLength, 0);
    return bytes;
}

PasskeyBytes buildCoseEC2PublicKey(const PasskeyPublicKey& publicKey)
{
    auto writer = CborWriter {};
    writer.beginMap(coseEC2KeyEntryCount);
    writer.writeSignedInteger(coseKeyLabelKeyType);
    writer.writeSignedInteger(coseKeyTypeEC2);
    writer.writeSignedInteger(coseKeyLabelAlgorithm);
    writer.writeSignedInteger(coseAlgorithmES256);
    writer.writeSignedInteger(coseKeyLabelCurve);
    writer.writeSignedInteger(coseCurveP256);
    writer.writeSignedInteger(coseKeyLabelX);
    writer.writeByteString(publicKey.x);
    writer.writeSignedInteger(coseKeyLabelY);
    writer.writeByteString(publicKey.y);
    return writer.getBytes();
}

PasskeyBytes buildAttestedCredentialData(const PasskeyBytes& credentialId,
                                         const PasskeyPublicKey& publicKey)
{
    auto bytes = PasskeyBytes(aaguidLength, 0);
    auto idLength = credentialId.size();
    auto idLengthHighByte =
        static_cast<std::uint8_t>(idLength >> credentialIdLengthHighByteShift);
    auto idLengthLowByte = static_cast<std::uint8_t>(idLength & lowestByteMask);
    bytes.push_back(idLengthHighByte);
    bytes.push_back(idLengthLowByte);
    append(bytes, credentialId);
    append(bytes, buildCoseEC2PublicKey(publicKey));
    return bytes;
}
} // namespace

std::string buildClientDataJSON(PasskeyCeremony ceremony,
                                const PasskeyBytes& challenge,
                                const std::string& origin)
{
    auto type = std::string("webauthn.get");

    if (ceremony == PasskeyCeremony::creation)
        type = "webauthn.create";

    // WebAuthn §5.8.1.2 fixes this member order, so the JSON is written by hand.
    return R"({"type":")" + type + R"(","challenge":")" + encodeBase64Url(challenge)
           + R"(","origin":)" + Miro::Json::print(Miro::Json::Value(origin))
           + R"(,"crossOrigin":false})";
}

PasskeyBytes buildCreationAuthenticatorData(const std::string& relyingPartyId,
                                            const PasskeyBytes& credentialId,
                                            const PasskeyPublicKey& publicKey)
{
    auto flags = static_cast<std::uint8_t>(flagUserPresent | flagUserVerified
                                           | flagAttestedCredentialData);
    auto bytes = buildAuthenticatorDataHeader(relyingPartyId, flags);
    append(bytes, buildAttestedCredentialData(credentialId, publicKey));
    return bytes;
}

PasskeyBytes buildAssertionAuthenticatorData(const std::string& relyingPartyId)
{
    auto flags = static_cast<std::uint8_t>(flagUserPresent | flagUserVerified);
    return buildAuthenticatorDataHeader(relyingPartyId, flags);
}

PasskeyBytes buildAttestationObject(const PasskeyBytes& authenticatorData)
{
    auto writer = CborWriter {};
    writer.beginMap(attestationObjectEntryCount);
    writer.writeTextString("fmt");
    writer.writeTextString(noneAttestationFormat);
    writer.writeTextString("attStmt");
    writer.beginMap(0);
    writer.writeTextString("authData");
    writer.writeByteString(authenticatorData);
    return writer.getBytes();
}

PasskeyBytes buildBytesToSignForAssertion(const PasskeyBytes& authenticatorData,
                                          const std::string& clientDataJSON)
{
    auto bytes = authenticatorData;
    append(bytes, computeSha256OfText(clientDataJSON));
    return bytes;
}
} // namespace grazer
