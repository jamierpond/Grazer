#include <Passkey.h>

#include <Miro/Json.h>
#include <NanoTest/NanoTest.h>
#include <eacp/Core/Utils/Strings.h>

#include <map>
#include <variant>

using namespace nano;
using namespace grazer;

namespace
{
constexpr auto exampleOrigin = "https://example.com";
constexpr auto exampleRelyingPartyIdHashHex =
    "a379a6f6eeafb9a55e378c118034e2751e682fab9f2d30ab13d2125586ce1947";

constexpr auto userPresentAndVerifiedFlagsHex = "05";
constexpr auto withAttestedCredentialDataFlagsHex = "45";
constexpr auto zeroSignatureCounterHex = "00000000";
constexpr auto zeroAaguidHex = "00000000000000000000000000000000";

constexpr auto creationRequestId = 7;
constexpr auto rsaOnlyCreationRequestId = 8;
constexpr auto assertionRequestId = 9;
constexpr auto coseAlgorithmRS256 = -257;
constexpr auto attestationObjectEntryCount = std::size_t {3};

constexpr auto coordinateLength = std::size_t {32};
constexpr auto xCoordinateByte = std::uint8_t {0x11};
constexpr auto yCoordinateByte = std::uint8_t {0x22};

constexpr auto creationRequestJSON = R"({
    "requestId": 7,
    "operation": "create",
    "publicKey": {
        "challenge": "AQIDBA",
        "rp": {"name": "Example"},
        "user": {"id": "dXNlcg", "name": "jamie@example.com", "displayName": "Jamie"},
        "pubKeyCredParams": [{"type": "public-key", "alg": -7},
                             {"type": "public-key", "alg": -257}],
        "excludeCredentials": [{"type": "public-key", "id": "CQk"}],
        "authenticatorSelection": {"userVerification": "required"}
    }
})";

constexpr auto rsaOnlyCreationRequestJSON = R"({
    "requestId": 8,
    "publicKey": {
        "challenge": "AQIDBA",
        "rp": {"id": "example.com", "name": "Example"},
        "user": {"id": "dXNlcg", "name": "jamie", "displayName": "Jamie"},
        "pubKeyCredParams": [{"type": "public-key", "alg": -257}]
    }
})";

constexpr auto assertionRequestJSON = R"({
    "requestId": 9,
    "operation": "get",
    "publicKey": {
        "challenge": "AQIDBA",
        "rpId": "example.com",
        "allowCredentials": [{"type": "public-key", "id": "AQID"}],
        "userVerification": "preferred"
    }
})";

PasskeyBytes makeBytes(std::size_t count, std::uint8_t value)
{
    return PasskeyBytes(count, value);
}

PasskeyPublicKey makeTestPublicKey()
{
    return {makeBytes(coordinateLength, xCoordinateByte),
            makeBytes(coordinateLength, yCoordinateByte)};
}

std::string toHex(const PasskeyBytes& bytes)
{
    return eacp::Strings::toHex(bytes);
}

std::string repeatHex(const std::string& byteHex, std::size_t count)
{
    auto text = std::string {};

    for (auto index = std::size_t {0}; index < count; ++index)
        text += byteHex;

    return text;
}

class FakeKeyStore : public PasskeyKeyStore
{
public:
    std::optional<CreatedPasskey> createKey(const std::string& relyingPartyId,
                                            const PasskeyUser& user) override
    {
        ++createdKeyCount;
        auto credential = PasskeyCredential {{1, 2, 3}, relyingPartyId, user};
        return CreatedPasskey {credential, makeTestPublicKey()};
    }

    std::vector<PasskeyCredential>
        listCredentials(const std::string& relyingPartyId) override
    {
        auto matching = std::vector<PasskeyCredential> {};

        for (auto& credential: heldCredentials)
        {
            if (credential.relyingPartyId == relyingPartyId)
                matching.push_back(credential);
        }

        return matching;
    }

    std::optional<PasskeyBytes>
        signWithCredential(const PasskeyBytes& credentialId,
                           const PasskeyBytes& bytesToSign) override
    {
        signedCredentialId = credentialId;
        signedBytes = bytesToSign;
        return PasskeyBytes {0x30, 0x00};
    }

    std::vector<PasskeyCredential> heldCredentials;
    int createdKeyCount = 0;
    PasskeyBytes signedCredentialId;
    PasskeyBytes signedBytes;
};

PasskeyCredential makeHeldCredential(const PasskeyBytes& credentialId)
{
    return {credentialId, "example.com", {{'u', 's', 'e', 'r'}, "jamie", "Jamie"}};
}

// Just enough CBOR reading to take an attestation object apart again.
struct CborValue
{
    std::variant<std::int64_t,
                 PasskeyBytes,
                 std::string,
                 std::map<std::string, CborValue>>
        value;
};

class CborReader
{
public:
    explicit CborReader(const PasskeyBytes& bytesToRead)
        : bytes(bytesToRead)
    {
    }

    CborValue readValue()
    {
        constexpr auto majorTypeShift = 5;
        constexpr auto additionalInformationMask = 0x1f;

        auto initialByte = bytes.at(position++);
        auto majorType = initialByte >> majorTypeShift;
        auto argument = readArgument(initialByte & additionalInformationMask);

        return readValueOfMajorType(majorType, argument);
    }

    bool isAtEnd() const { return position == bytes.size(); }

private:
    std::uint64_t readArgument(int additionalInformation)
    {
        constexpr auto largestInlineArgument = 23;
        constexpr auto firstFollowingLengthCode = 24;
        constexpr auto bitsPerByte = 8;

        if (additionalInformation <= largestInlineArgument)
            return static_cast<std::uint64_t>(additionalInformation);

        auto byteCount = std::size_t {1}
                         << (additionalInformation - firstFollowingLengthCode);
        auto argument = std::uint64_t {0};

        for (auto index = std::size_t {0}; index < byteCount; ++index)
            argument = (argument << bitsPerByte) | bytes.at(position++);

        return argument;
    }

    CborValue readValueOfMajorType(int majorType, std::uint64_t argument)
    {
        constexpr auto majorTypeUnsignedInteger = 0;
        constexpr auto majorTypeNegativeInteger = 1;
        constexpr auto majorTypeByteString = 2;
        constexpr auto majorTypeTextString = 3;

        auto signedArgument = static_cast<std::int64_t>(argument);

        if (majorType == majorTypeUnsignedInteger)
            return {signedArgument};

        if (majorType == majorTypeNegativeInteger)
            return {-1 - signedArgument};

        if (majorType == majorTypeByteString)
            return {readBytes(argument)};

        if (majorType == majorTypeTextString)
        {
            auto text = readBytes(argument);
            return {std::string(text.begin(), text.end())};
        }

        return {readTextKeyedMap(argument)};
    }

    PasskeyBytes readBytes(std::uint64_t count)
    {
        auto offset = static_cast<std::ptrdiff_t>(position);
        auto first = bytes.begin() + offset;
        position += count;
        return {first, first + static_cast<std::ptrdiff_t>(count)};
    }

    std::map<std::string, CborValue> readTextKeyedMap(std::uint64_t entryCount)
    {
        auto entries = std::map<std::string, CborValue> {};

        for (auto index = std::uint64_t {0}; index < entryCount; ++index)
        {
            auto key = std::get<std::string>(readValue().value);
            entries[key] = readValue();
        }

        return entries;
    }

    const PasskeyBytes& bytes;
    std::size_t position = 0;
};

Miro::Json::Value parseReply(const std::string& reply)
{
    return Miro::Json::parse(reply);
}
} // namespace

auto tRelyingPartyIdMatchingTheHostIsAllowed =
    test("Passkey/an rp id equal to the origin's host is allowed") = []
{
    check(isRelyingPartyAllowedForOrigin("example.com", "https://example.com"));
    check(isRelyingPartyAllowedForOrigin("accounts.google.com",
                                         "https://accounts.google.com:443"));
};

auto tParentDomainIsAllowed =
    test("Passkey/an rp id that is a parent domain of the host is allowed") = []
{
    check(
        isRelyingPartyAllowedForOrigin("google.com", "https://accounts.google.com"));
    check(isRelyingPartyAllowedForOrigin("example.co.uk",
                                         "https://login.example.co.uk"));
};

auto tUnrelatedHostsAreRefused =
    test("Passkey/an rp id for another site is refused") = []
{
    check(!isRelyingPartyAllowedForOrigin("google.com", "https://evil.com"));
    check(!isRelyingPartyAllowedForOrigin("ample.com", "https://example.com"));
    check(!isRelyingPartyAllowedForOrigin("mail.google.com",
                                          "https://accounts.google.com"));
    check(!isRelyingPartyAllowedForOrigin("accounts.google.com",
                                          "https://google.com"));
    check(!isRelyingPartyAllowedForOrigin("", "https://example.com"));
};

auto tDotlessSuffixIsRefused =
    test("Passkey/a top-level domain alone is refused as an rp id") = []
{ check(!isRelyingPartyAllowedForOrigin("com", "https://example.com")); };

auto tIPAddressSuffixIsRefused =
    test("Passkey/a suffix of an IP address is refused as an rp id") = []
{ check(!isRelyingPartyAllowedForOrigin("0.0.1", "https://10.0.0.1")); };

auto tOnlySecureOriginsAreAllowed =
    test("Passkey/plain http is refused except on localhost") = []
{
    check(!isRelyingPartyAllowedForOrigin("example.com", "http://example.com"));
    check(!isRelyingPartyAllowedForOrigin("example.com", "ftp://example.com"));
    check(!isRelyingPartyAllowedForOrigin("example.com", "example.com"));
    check(isRelyingPartyAllowedForOrigin("localhost", "http://localhost:5173"));
};

auto tMissingRelyingPartyIdIsTheHost =
    test("Passkey/a request naming no rp id uses the origin's host") = []
{
    check(resolveRelyingPartyId(std::nullopt, "https://login.example.com:8443")
          == "login.example.com");
    check(resolveRelyingPartyId("example.com", "https://login.example.com")
          == "example.com");
    check(!resolveRelyingPartyId("other.com", "https://login.example.com"));
};

auto tCreationRequestIsParsed =
    test("Passkey/a creation request is read from the page's JSON") = []
{
    auto request = parsePasskeyCreationRequest(creationRequestJSON);

    check(request.has_value());
    check(request->requestId == creationRequestId);
    check(request->challenge == PasskeyBytes {1, 2, 3, 4});
    check(!request->relyingPartyId.has_value());
    check(request->relyingPartyName == "Example");
    check(request->user.id == PasskeyBytes {'u', 's', 'e', 'r'});
    check(request->user.name == "jamie@example.com");
    check(request->user.displayName == "Jamie");
    check(request->algorithms
          == std::vector<int> {coseAlgorithmES256, coseAlgorithmRS256});
    check(request->excludedCredentialIds == std::vector<PasskeyBytes> {{9, 9}});
    check(request->userVerification == "required");
};

auto tAssertionRequestIsParsed =
    test("Passkey/an assertion request is read from the page's JSON") = []
{
    auto request = parsePasskeyAssertionRequest(assertionRequestJSON);

    check(request.has_value());
    check(request->requestId == assertionRequestId);
    check(request->challenge == PasskeyBytes {1, 2, 3, 4});
    check(request->relyingPartyId == "example.com");
    check(request->allowedCredentialIds == std::vector<PasskeyBytes> {{1, 2, 3}});
    check(request->userVerification == "preferred");
};

auto tMessageHeaderNamesTheOperation =
    test("Passkey/the message header says which operation the page wants") = []
{
    auto header = readPasskeyMessageHeader(assertionRequestJSON);

    check(header.requestId == assertionRequestId);
    check(header.operation == PasskeyOperation::get);
    check(readPasskeyMessageHeader(R"({"operation":"steal"})").operation
          == PasskeyOperation::unknown);
};

auto tRequestWithoutChallengeIsRejected =
    test("Passkey/a request without a usable challenge is not parsed") = []
{
    check(!parsePasskeyAssertionRequest(R"({"publicKey":{"rpId":"example.com"}})"));
    check(!parsePasskeyAssertionRequest(
        R"({"publicKey":{"challenge":"not base64!"}})"));
    check(!parsePasskeyCreationRequest("not json"));
};

auto tCreationWithoutES256IsRefused =
    test("Passkey/a creation request that does not offer ES256 is refused") = []
{
    auto request = parsePasskeyCreationRequest(rsaOnlyCreationRequestJSON);
    auto keyStore = FakeKeyStore {};

    check(!isES256Offered(*request));

    auto reply =
        parseReply(answerPasskeyCreationRequest(*request, exampleOrigin, keyStore));

    check(reply["requestId"].asInteger() == rsaOnlyCreationRequestId);
    check(reply["error"]["name"].asString() == "NotSupportedError");
    check(keyStore.createdKeyCount == 0);
};

auto tExcludedCredentialIsRefused = test(
    "Passkey/creating a passkey the site already has with Grazer is refused") = []
{
    auto request = parsePasskeyCreationRequest(creationRequestJSON);
    auto keyStore = FakeKeyStore {};
    keyStore.heldCredentials = {makeHeldCredential({9, 9})};

    auto reply =
        parseReply(answerPasskeyCreationRequest(*request, exampleOrigin, keyStore));

    check(reply["error"]["name"].asString() == "InvalidStateError");
    check(keyStore.createdKeyCount == 0);
};

auto tCreationFromForeignOriginIsRefused = test(
    "Passkey/creating a passkey for another site's rp id is a SecurityError") = []
{
    auto request = parsePasskeyCreationRequest(rsaOnlyCreationRequestJSON);
    auto keyStore = FakeKeyStore {};

    auto reply = parseReply(
        answerPasskeyCreationRequest(*request, "https://evil.com", keyStore));

    check(reply["error"]["name"].asString() == "SecurityError");
};

auto tCreationReplyCarriesTheCredential =
    test("Passkey/a granted creation replies with the new credential") = []
{
    auto request = parsePasskeyCreationRequest(creationRequestJSON);
    auto keyStore = FakeKeyStore {};

    auto reply =
        parseReply(answerPasskeyCreationRequest(*request, exampleOrigin, keyStore));
    auto& response = reply["credential"]["response"];

    check(reply["requestId"].asInteger() == creationRequestId);
    check(reply["credential"]["id"].asString() == "AQID");
    check(response["publicKeyAlgorithm"].asInteger() == coseAlgorithmES256);
    check(response["transports"][0].asString() == "internal");
    check(!response["attestationObject"].asString().empty());
    check(!response["publicKey"].asString().empty());
};

auto tClientDataIsInSpecOrder =
    test("Passkey/clientDataJSON lists type, challenge, origin and crossOrigin") = []
{
    auto clientData =
        buildClientDataJSON(PasskeyCeremony::creation, {1, 2, 3, 4}, exampleOrigin);

    check(clientData
          == R"({"type":"webauthn.create","challenge":"AQIDBA",)"
             R"("origin":"https://example.com","crossOrigin":false})");
};

auto tAssertionAuthenticatorDataMatchesVector = test(
    "Passkey/assertion authenticator data is the rp id hash, UP+UV, counter 0") = []
{
    auto expected = std::string(exampleRelyingPartyIdHashHex)
                    + userPresentAndVerifiedFlagsHex + zeroSignatureCounterHex;

    check(toHex(buildAssertionAuthenticatorData("example.com")) == expected);
};

auto tCreationAuthenticatorDataMatchesVector =
    test("Passkey/creation authenticator data carries the COSE public key") = []
{
    auto credentialId = PasskeyBytes {1, 2, 3, 4};
    auto authenticatorData = buildCreationAuthenticatorData(
        "example.com", credentialId, makeTestPublicKey());

    // COSE_Key map(5): 1:2 (EC2), 3:-7 (ES256), -1:1 (P-256), -2:x, -3:y.
    auto coseKeyHex = "a5010203262001215820" + repeatHex("11", coordinateLength)
                      + "225820" + repeatHex("22", coordinateLength);
    auto credentialIdLengthAndIdHex = "000401020304";
    auto expected = std::string(exampleRelyingPartyIdHashHex)
                    + withAttestedCredentialDataFlagsHex + zeroSignatureCounterHex
                    + zeroAaguidHex + credentialIdLengthAndIdHex + coseKeyHex;

    check(toHex(authenticatorData) == expected);
};

auto tAttestationObjectDecodes = test(
    "Passkey/the attestation object decodes to fmt none, no statement, authData") =
    []
{
    auto authenticatorData = buildCreationAuthenticatorData(
        "example.com", {1, 2, 3, 4}, makeTestPublicKey());
    auto attestationObject = buildAttestationObject(authenticatorData);

    auto reader = CborReader {attestationObject};
    auto decoded =
        std::get<std::map<std::string, CborValue>>(reader.readValue().value);

    check(reader.isAtEnd());
    check(decoded.size() == attestationObjectEntryCount);
    check(std::get<std::string>(decoded["fmt"].value) == "none");
    check(std::get<std::map<std::string, CborValue>>(decoded["attStmt"].value)
              .empty());
    check(std::get<PasskeyBytes>(decoded["authData"].value) == authenticatorData);
};

auto tAssertionSignsAuthenticatorDataAndClientDataHash = test(
    "Passkey/an assertion signs authenticator data then SHA-256 of client data") = []
{
    constexpr auto clientDataHashHex =
        "02f3f5d6c3747d989eea71bcccf868f872f6a964d1ad00fb1c8760550c56c34e";

    auto authenticatorData = buildAssertionAuthenticatorData("example.com");
    auto clientData =
        buildClientDataJSON(PasskeyCeremony::assertion, {1, 2, 3, 4}, exampleOrigin);
    auto bytesToSign = buildBytesToSignForAssertion(authenticatorData, clientData);

    check(toHex(bytesToSign) == toHex(authenticatorData) + clientDataHashHex);
};

auto tAssertionAsksTheStoreToSignThoseBytes =
    test("Passkey/a granted assertion signs with the chosen credential") = []
{
    auto request = parsePasskeyAssertionRequest(assertionRequestJSON);
    auto keyStore = FakeKeyStore {};
    keyStore.heldCredentials = {makeHeldCredential({1, 2, 3}),
                                makeHeldCredential({4, 5, 6})};

    auto found = findCredentialsForAssertion(*request, exampleOrigin, keyStore);
    auto& candidates = std::get<std::vector<PasskeyCredential>>(found);

    check(candidates.size() == 1);

    auto reply = parseReply(answerPasskeyAssertionRequest(
        *request, exampleOrigin, candidates.front(), keyStore));
    auto clientData =
        buildClientDataJSON(PasskeyCeremony::assertion, {1, 2, 3, 4}, exampleOrigin);
    auto expectedBytes = buildBytesToSignForAssertion(
        buildAssertionAuthenticatorData("example.com"), clientData);

    check(keyStore.signedCredentialId == PasskeyBytes {1, 2, 3});
    check(keyStore.signedBytes == expectedBytes);
    check(reply["credential"]["id"].asString() == "AQID");
    check(reply["credential"]["response"]["userHandle"].asString() == "dXNlcg");
    check(reply["credential"]["response"]["signature"].asString() == "MAA");
};

auto tAssertionWithNoHeldCredentialIsNotAllowed = test(
    "Passkey/an assertion with no passkey held for the site is NotAllowedError") = []
{
    auto request = parsePasskeyAssertionRequest(assertionRequestJSON);
    auto keyStore = FakeKeyStore {};

    auto found = findCredentialsForAssertion(*request, exampleOrigin, keyStore);

    check(std::get<PasskeyError>(found).name == PasskeyErrorName::notAllowed);
};

auto tErrorReplyNamesTheDomException =
    test("Passkey/an error reply carries the DOMException name and message") = []
{
    constexpr auto requestId = 3;
    auto reply = parseReply(buildPasskeyErrorReply(
        requestId, {PasskeyErrorName::security, "wrong site"}));

    check(reply["requestId"].asInteger() == requestId);
    check(reply["error"]["name"].asString() == "SecurityError");
    check(reply["error"]["message"].asString() == "wrong site");
};
