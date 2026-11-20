#include "PlatformPasskeyStore.h"

namespace PlatformPasskeyStore
{
namespace
{
class UnavailableKeyStore final : public RequestKeyStore
{
public:
    std::optional<grazer::CreatedPasskey> createKey(const std::string&,
                                                    const grazer::PasskeyUser&) override
    {
        return std::nullopt;
    }

    std::vector<grazer::PasskeyCredential> listCredentials(const std::string&) override
    {
        return {};
    }

    std::optional<grazer::PasskeyBytes>
        signWithCredential(const grazer::PasskeyCredential&,
                           const grazer::PasskeyBytes&) override
    {
        return std::nullopt;
    }

    void cancelUserPrompt() override {}
};
} // namespace

bool isAvailable()
{
    return false;
}

std::shared_ptr<RequestKeyStore> createRequestKeyStore()
{
    return std::make_shared<UnavailableKeyStore>();
}

} // namespace PlatformPasskeyStore
