#pragma once

#include "PlatformPasskeyStore.h"

#include <eacp/WebView/WebView.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Answers the passkey page script's requests from every tab, when this Grazer
// can keep passkeys (PlatformPasskeyStore::isAvailable). The keychain and
// Touch ID run on eacp's thread pool; each reply goes back on the main thread,
// and only to a tab still on the origin that asked.
class PasskeyRequests
{
public:
    using TabView = std::optional<std::reference_wrapper<eacp::Graphics::WebView>>;

    // The tab's web view, or nullopt once the tab has closed.
    using TabFinder = std::function<TabView(std::int64_t tabId)>;

    explicit PasskeyRequests(TabFinder tabFinderToUse);

    // For a tab's own web view; a popup shares its opener's page scripts.
    static void addPageScript(eacp::Graphics::WebView& view);

    void receiveRequestsFrom(eacp::Graphics::WebView& view, std::int64_t tabId);

private:
    struct Requester
    {
        std::int64_t tabId = 0;
        std::string origin;
    };

    struct PageMessage
    {
        Requester requester;
        std::string text;
    };

    struct PendingRequest
    {
        Requester requester;
        int requestId = 0;
        std::shared_ptr<PlatformPasskeyStore::RequestKeyStore> keyStore;
    };

    void handleMessage(const PageMessage& message);
    void startCreation(const PageMessage& message);
    void startAssertion(const PageMessage& message);
    void continueAssertion(const PendingRequest& pending,
                           const grazer::PasskeyAssertionRequest& request,
                           const std::vector<grazer::PasskeyCredential>& credentials);
    void abortRequest(const PageMessage& message);
    void replyUnreadable(const PageMessage& message);

    PendingRequest addPendingRequest(const PageMessage& message);
    bool isStillPending(const PendingRequest& pending) const;
    void finishRequest(const PendingRequest& pending, const std::string& reply);
    void replyToTab(const Requester& requester, const std::string& reply);

    TabFinder tabFinder;
    std::vector<PendingRequest> pendingRequests;
};
