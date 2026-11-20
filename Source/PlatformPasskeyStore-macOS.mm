#include "PlatformPasskeyStore.h"

#include <eacp/Core/ObjC/AutoReleasePool.h>
#include <eacp/Core/ObjC/CFRef.h>
#include <eacp/Core/ObjC/ObjC.h>
#include <eacp/Core/ObjC/Strings.h>

#include <atomic>

#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/SecTask.h>
#import <Security/Security.h>

namespace PlatformPasskeyStore
{
namespace
{
using grazer::CreatedPasskey;
using grazer::PasskeyBytes;
using grazer::PasskeyCredential;
using grazer::PasskeyPublicKey;
using grazer::PasskeyUser;

using CFDictionary = eacp::CFRef<CFMutableDictionaryRef>;

constexpr auto applicationIdentifierEntitlement = "com.apple.application-identifier";
constexpr auto keychainAccessGroupsEntitlement = "keychain-access-groups";

// Every Grazer passkey's key item carries this label, so a query finds them all
// and nothing else in the access group.
constexpr auto passkeyKeyLabel = "Grazer passkey";

constexpr auto p256KeySizeInBits = 256;
constexpr auto credentialIdLength = std::size_t {16};
constexpr auto p256CoordinateLength = std::size_t {32};
constexpr auto uncompressedPointMarkerLength = std::size_t {1};
constexpr auto uncompressedPointLength =
    uncompressedPointMarkerLength + 2 * p256CoordinateLength;

eacp::CFRef<CFStringRef> createString(const std::string& text)
{
    return CFStringCreateWithCString(
        kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
}

eacp::CFRef<CFDataRef> createData(const PasskeyBytes& bytes)
{
    auto length = static_cast<CFIndex>(bytes.size());
    return CFDataCreate(kCFAllocatorDefault, bytes.data(), length);
}

PasskeyBytes readBytes(CFDataRef data)
{
    auto* first = CFDataGetBytePtr(data);
    auto length = static_cast<std::size_t>(CFDataGetLength(data));
    return {first, first + length};
}

std::string readString(CFStringRef text)
{
    auto* string = (NSString*) text;
    return eacp::Strings::toStdString(string);
}

CFDictionary createDictionary()
{
    return CFDictionaryCreateMutable(kCFAllocatorDefault,
                                     0,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
}

eacp::CFRef<CFTypeRef> copyOwnEntitlement(const std::string& name)
{
    auto task = eacp::CFRef<SecTaskRef> {SecTaskCreateFromSelf(kCFAllocatorDefault)};

    if (!task)
        return {};

    auto entitlementName = createString(name);
    return SecTaskCopyValueForEntitlement(task, entitlementName, nullptr);
}

std::optional<std::string> readFirstKeychainAccessGroup()
{
    auto groups = copyOwnEntitlement(keychainAccessGroupsEntitlement);

    if (!groups || CFGetTypeID(groups) != CFArrayGetTypeID())
        return std::nullopt;

    auto groupList = static_cast<CFArrayRef>(groups.get());

    if (CFArrayGetCount(groupList) == 0)
        return std::nullopt;

    auto firstGroup = CFArrayGetValueAtIndex(groupList, 0);

    if (CFGetTypeID(firstGroup) != CFStringGetTypeID())
        return std::nullopt;

    auto firstGroupName = static_cast<CFStringRef>(firstGroup);
    return readString(firstGroupName);
}

// The access group Grazer's signature claims, when it is signed with an
// application identifier; nullopt for an ad-hoc build.
std::optional<std::string> readSignedKeychainAccessGroup()
{
    if (!copyOwnEntitlement(applicationIdentifierEntitlement))
        return std::nullopt;

    return readFirstKeychainAccessGroup();
}

const std::optional<std::string>& getKeychainAccessGroup()
{
    static const auto accessGroup = readSignedKeychainAccessGroup();
    return accessGroup;
}

bool hasSecureEnclave()
{
    eacp::ObjC::AutoReleasePool pool;
    auto watcher = eacp::ObjC::makePtr<TKTokenWatcher>();
    auto* secureEnclaveTokenId = (NSString*) kSecAttrTokenIDSecureEnclave;
    return [[watcher.get() tokenIDs] containsObject:secureEnclaveTokenId];
}

// The query that finds every Grazer passkey's private key, without reading any
// key material.
CFDictionary createPasskeyQuery(const std::string& accessGroup)
{
    auto query = createDictionary();
    auto group = createString(accessGroup);
    auto label = createString(passkeyKeyLabel);

    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    CFDictionarySetValue(query, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(query, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);
    CFDictionarySetValue(query, kSecAttrAccessGroup, group);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    CFDictionarySetValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    return query;
}

std::optional<PasskeyCredential> readCredentialOfKeyItem(CFDictionaryRef item)
{
    auto tag = CFDictionaryGetValue(item, kSecAttrApplicationTag);

    if (tag == nullptr || CFGetTypeID(tag) != CFDataGetTypeID())
        return std::nullopt;

    auto tagData = static_cast<CFDataRef>(tag);
    return grazer::decodePasskeyCredentialMetadata(readBytes(tagData));
}

// Each entry of a kSecMatchLimitAll result, as the item dictionary it is.
template <typename Visit>
void visitKeyItems(CFTypeRef result, const Visit& visit)
{
    if (result == nullptr || CFGetTypeID(result) != CFArrayGetTypeID())
        return;

    auto items = static_cast<CFArrayRef>(result);

    for (auto index = CFIndex {0}; index < CFArrayGetCount(items); ++index)
    {
        auto item = CFArrayGetValueAtIndex(items, index);

        if (CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;

        auto itemDictionary = static_cast<CFDictionaryRef>(item);
        visit(itemDictionary);
    }
}

std::optional<CFDictionaryRef> findKeyItemOfCredential(CFTypeRef result,
                                                      const PasskeyBytes& credentialId)
{
    auto found = std::optional<CFDictionaryRef> {};

    visitKeyItems(result,
                  [&](CFDictionaryRef item)
                  {
                      auto held = readCredentialOfKeyItem(item);

                      if (held && held->credentialId == credentialId)
                          found = item;
                  });

    return found;
}

eacp::CFRef<CFTypeRef> copyMatchingKeyItems(CFDictionaryRef query)
{
    auto result = CFTypeRef {nullptr};

    if (SecItemCopyMatching(query, &result) != errSecSuccess)
        return {};

    return result;
}

PasskeyBytes createRandomCredentialId()
{
    auto credentialId = PasskeyBytes(credentialIdLength);

    if (SecRandomCopyBytes(kSecRandomDefault, credentialIdLength, credentialId.data())
        != errSecSuccess)
    {
        return {};
    }

    return credentialId;
}

std::optional<PasskeyPublicKey> copyPublicKeyCoordinates(SecKeyRef privateKey)
{
    auto publicKey = eacp::CFRef<SecKeyRef> {SecKeyCopyPublicKey(privateKey)};

    if (!publicKey)
        return std::nullopt;

    auto point =
        eacp::CFRef<CFDataRef> {SecKeyCopyExternalRepresentation(publicKey, nullptr)};

    if (!point)
        return std::nullopt;

    auto bytes = readBytes(point);

    if (bytes.size() != uncompressedPointLength)
        return std::nullopt;

    auto xStart = bytes.begin() + uncompressedPointMarkerLength;
    auto yStart = xStart + p256CoordinateLength;
    return PasskeyPublicKey {{xStart, yStart}, {yStart, bytes.end()}};
}

eacp::CFRef<SecAccessControlRef> createPasskeyAccessControl()
{
    auto flags = kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence;
    return SecAccessControlCreateWithFlags(
        kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, flags, nullptr);
}

CFDictionary createPrivateKeyAttributes(const PasskeyCredential& credential)
{
    auto attributes = createDictionary();
    auto accessControl = createPasskeyAccessControl();
    auto tag = createData(grazer::encodePasskeyCredentialMetadata(credential));
    auto label = createString(passkeyKeyLabel);

    CFDictionarySetValue(attributes, kSecAttrIsPermanent, kCFBooleanTrue);
    CFDictionarySetValue(attributes, kSecAttrAccessControl, accessControl);
    CFDictionarySetValue(attributes, kSecAttrApplicationTag, tag);
    CFDictionarySetValue(attributes, kSecAttrLabel, label);
    return attributes;
}

class SecureEnclaveKeyStore final : public RequestKeyStore
{
public:
    explicit SecureEnclaveKeyStore(std::string accessGroupToUse)
        : accessGroup(std::move(accessGroupToUse))
    {
    }

    std::optional<CreatedPasskey> createKey(const std::string& relyingPartyId,
                                            const PasskeyUser& user) override
    {
        eacp::ObjC::AutoReleasePool pool;

        if (!askForUserPresence(grazer::describePasskeyCreationReason(relyingPartyId, user)))
            return std::nullopt;

        auto credential = PasskeyCredential {createRandomCredentialId(), relyingPartyId, user};

        if (credential.credentialId.empty())
            return std::nullopt;

        auto privateKey = createSecureEnclaveKey(credential);

        if (!privateKey)
            return std::nullopt;

        auto publicKey = copyPublicKeyCoordinates(privateKey);

        if (!publicKey)
            return std::nullopt;

        return CreatedPasskey {credential, *publicKey};
    }

    std::vector<PasskeyCredential>
        listCredentials(const std::string& relyingPartyId) override
    {
        auto query = createPasskeyQuery(accessGroup);
        auto result = copyMatchingKeyItems(query);
        auto credentials = std::vector<PasskeyCredential> {};

        visitKeyItems(result,
                      [&](CFDictionaryRef item)
                      {
                          auto credential = readCredentialOfKeyItem(item);

                          if (credential && credential->relyingPartyId == relyingPartyId)
                              credentials.push_back(*credential);
                      });

        return credentials;
    }

    // macOS shows the Touch ID sheet, with the context's reason, when the
    // signature first uses the key.
    std::optional<PasskeyBytes> signWithCredential(const PasskeyCredential& credential,
                                                   const PasskeyBytes& bytesToSign) override
    {
        eacp::ObjC::AutoReleasePool pool;

        if (isCancelled)
            return std::nullopt;

        auto reason = grazer::describePasskeySigningReason(credential);
        [context.get() setLocalizedReason:eacp::Strings::toNSString(reason)];

        auto query = createPasskeyQuery(accessGroup);
        CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
        CFDictionarySetValue(query, kSecUseAuthenticationContext, context.get());

        auto result = copyMatchingKeyItems(query);
        auto item = findKeyItemOfCredential(result, credential.credentialId);

        if (!item)
            return std::nullopt;

        return signWithKeyItem(*item, bytesToSign);
    }

    void cancelUserPrompt() override
    {
        isCancelled = true;
        [context.get() invalidate];
    }

private:
    bool askForUserPresence(const std::string& reason)
    {
        if (isCancelled)
            return false;

        auto semaphore = dispatch_semaphore_create(0);
        __block auto isPresent = false;

        [context.get() evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                      localizedReason:eacp::Strings::toNSString(reason)
                                reply:^(BOOL success, NSError*) {
                                  isPresent = success;
                                  dispatch_semaphore_signal(semaphore);
                                }];

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        dispatch_release(semaphore);
        return isPresent && !isCancelled;
    }

    eacp::CFRef<SecKeyRef> createSecureEnclaveKey(const PasskeyCredential& credential)
    {
        auto attributes = createDictionary();
        auto privateKeyAttributes = createPrivateKeyAttributes(credential);
        auto group = createString(accessGroup);
        auto keySize = p256KeySizeInBits;
        auto keySizeNumber = eacp::CFRef<CFNumberRef> {
            CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize)};

        CFDictionarySetValue(attributes, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
        CFDictionarySetValue(attributes, kSecAttrKeySizeInBits, keySizeNumber);
        CFDictionarySetValue(attributes, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);
        CFDictionarySetValue(attributes, kSecUseDataProtectionKeychain, kCFBooleanTrue);
        CFDictionarySetValue(attributes, kSecAttrAccessGroup, group);
        CFDictionarySetValue(attributes, kSecUseAuthenticationContext, context.get());
        CFDictionarySetValue(attributes, kSecPrivateKeyAttrs, privateKeyAttributes);

        return SecKeyCreateRandomKey(attributes, nullptr);
    }

    std::optional<PasskeyBytes> signWithKeyItem(CFDictionaryRef item,
                                                const PasskeyBytes& bytesToSign)
    {
        auto keyValue = CFDictionaryGetValue(item, kSecValueRef);

        if (keyValue == nullptr || CFGetTypeID(keyValue) != SecKeyGetTypeID())
            return std::nullopt;

        auto* mutableKeyValue = const_cast<void*>(keyValue);
        auto key = static_cast<SecKeyRef>(mutableKeyValue);
        auto message = createData(bytesToSign);
        auto signature = eacp::CFRef<CFDataRef> {SecKeyCreateSignature(
            key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, message, nullptr)};

        if (!signature || isCancelled)
            return std::nullopt;

        return readBytes(signature);
    }

    std::string accessGroup;
    eacp::ObjC::Ptr<LAContext> context = eacp::ObjC::makePtr<LAContext>();
    std::atomic<bool> isCancelled = false;
};
} // namespace

bool isAvailable()
{
    static const auto isStoreAvailable =
        getKeychainAccessGroup().has_value() && hasSecureEnclave();
    return isStoreAvailable;
}

std::shared_ptr<RequestKeyStore> createRequestKeyStore()
{
    return std::make_shared<SecureEnclaveKeyStore>(getKeychainAccessGroup().value_or(""));
}

} // namespace PlatformPasskeyStore
