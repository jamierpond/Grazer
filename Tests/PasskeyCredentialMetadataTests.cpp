#include <Passkey.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

namespace
{
PasskeyCredential makeCredential()
{
    return {{1, 2, 3}, "example.com", {{'u', 's', 'e', 'r'}, "jamie@example.com", "Jamie"}};
}

PasskeyBytes toBytes(const std::string& text)
{
    return {text.begin(), text.end()};
}
} // namespace

auto tMetadataRoundTrips =
    test("PasskeyMetadata/a credential reads back from its stored metadata") = []
{
    auto credential = makeCredential();
    auto decoded =
        decodePasskeyCredentialMetadata(encodePasskeyCredentialMetadata(credential));

    check(decoded.has_value());
    check(decoded->credentialId == credential.credentialId);
    check(decoded->relyingPartyId == credential.relyingPartyId);
    check(decoded->user.id == credential.user.id);
    check(decoded->user.name == credential.user.name);
    check(decoded->user.displayName == credential.user.displayName);
};

auto tForeignMetadataIsIgnored =
    test("PasskeyMetadata/bytes Grazer did not write are not a credential") = []
{
    check(!decodePasskeyCredentialMetadata({}));
    check(!decodePasskeyCredentialMetadata(toBytes("not json")));
    check(!decodePasskeyCredentialMetadata(
        toBytes(R"({"credentialId":"AQID","relyingPartyId":"example.com"})")));
    check(!decodePasskeyCredentialMetadata(toBytes(
        R"({"grazerPasskeyFormat":1,"credentialId":"","relyingPartyId":"example.com"})")));
    check(!decodePasskeyCredentialMetadata(
        toBytes(R"({"grazerPasskeyFormat":1,"credentialId":"AQID"})")));
};

auto tAccountNamesBothNames =
    test("PasskeyMetadata/an account reads as display name then user name") = []
{
    check(describePasskeyAccount({{}, "jamie@example.com", "Jamie"})
          == "Jamie (jamie@example.com)");
    check(describePasskeyAccount({{}, "jamie@example.com", ""}) == "jamie@example.com");
    check(describePasskeyAccount({{}, "jamie", "jamie"}) == "jamie");
    check(describePasskeyAccount({{}, "", "Jamie"}) == "Jamie");
    check(describePasskeyAccount({{}, "", ""}) == "an account with no name");
};

auto tReasonsNameTheSiteAndAccount =
    test("PasskeyMetadata/the Touch ID reasons name the site and the account") = []
{
    auto credential = makeCredential();

    check(describePasskeyCreationReason("example.com", credential.user)
          == "save a passkey for Jamie (jamie@example.com) on example.com");
    check(describePasskeySigningReason(credential)
          == "sign in to example.com as Jamie (jamie@example.com)");
};
