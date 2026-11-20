#include "PasskeyRequests.h"

#include "BrowserEngine.h"
#include "PasskeyChooser.h"
#include "PasskeyPageScript.h"

#include <eacp/Core/Threads/ThreadPool.h>
#include <eacp/Core/Utils/Logging.h>
#include <SessionLog/EventLine.h>

#include <algorithm>
#include <variant>

using namespace grazer;

namespace
{
using FoundCredentials = std::variant<std::vector<PasskeyCredential>, PasskeyError>;

PasskeyError makeUnreadableRequestError()
{
    return {PasskeyErrorName::notSupported, "Grazer could not read this passkey request."};
}

PasskeyError makeNoPasskeyChosenError()
{
    return {PasskeyErrorName::notAllowed, "No passkey was chosen."};
}

template <typename Requester>
void logPasskeyEvent(std::string_view event, const Requester& requester)
{
    eacp::LOG(sessionlog::formatEventLine(
        event, {{"tab", requester.tabId}, {"origin", requester.origin}}));
}

// Runs `work` on eacp's thread pool, then hands its result to `onMainThread`.
template <typename Result>
void runOffMainThread(std::function<Result()> work,
                      std::function<void(const Result&)> onMainThread)
{
    auto result = std::make_shared<std::optional<Result>>();

    eacp::Threads::ThreadPool::shared().submit(eacp::Threads::PoolTask {
        .work = [result, work] { *result = work(); },
        .onMainThread = [result, onMainThread] { onMainThread(**result); },
    });
}

// Each request has its own key store, so the store says which request it is.
template <typename Request>
auto isSameRequestAs(const Request& pending)
{
    return [&pending](const Request& held) { return held.keyStore == pending.keyStore; };
}

std::optional<PasskeyCredential>
    chooseCredential(const std::vector<PasskeyCredential>& credentials)
{
    if (credentials.size() == 1)
        return credentials.front();

    auto chosenIndex = askWhichPasskeyToUse(credentials);

    if (!chosenIndex)
        return std::nullopt;

    return credentials.at(*chosenIndex);
}
} // namespace

PasskeyRequests::PasskeyRequests(TabFinder tabFinderToUse)
    : tabFinder(std::move(tabFinderToUse))
{
}

void PasskeyRequests::addPageScript(eacp::Graphics::WebView& view)
{
    if (PlatformPasskeyStore::isAvailable())
        view.addUserScript(passkeyScriptSource, true);
}

void PasskeyRequests::receiveRequestsFrom(eacp::Graphics::WebView& view,
                                          std::int64_t tabId)
{
    if (!PlatformPasskeyStore::isAvailable())
        return;

    BrowserEngine::receivePasskeyMessages(
        view,
        [this, tabId](const std::string& text, const std::string& origin)
        { handleMessage({{tabId, origin}, text}); });
}

void PasskeyRequests::handleMessage(const PageMessage& message)
{
    auto operation = readPasskeyMessageHeader(message.text).operation;

    if (operation == PasskeyOperation::create)
        startCreation(message);
    else if (operation == PasskeyOperation::get)
        startAssertion(message);
    else if (operation == PasskeyOperation::abort)
        abortRequest(message);
}

void PasskeyRequests::startCreation(const PageMessage& message)
{
    auto request = parsePasskeyCreationRequest(message.text);

    if (!request)
    {
        replyUnreadable(message);
        return;
    }

    auto pending = addPendingRequest(message);
    logPasskeyEvent("passkey.create", message.requester);

    runOffMainThread<std::string>(
        [pending, creation = *request]
        {
            return answerPasskeyCreationRequest(
                creation, pending.requester.origin, *pending.keyStore);
        },
        [this, pending](const std::string& reply) { finishRequest(pending, reply); });
}

void PasskeyRequests::startAssertion(const PageMessage& message)
{
    auto request = parsePasskeyAssertionRequest(message.text);

    if (!request)
    {
        replyUnreadable(message);
        return;
    }

    auto pending = addPendingRequest(message);
    logPasskeyEvent("passkey.get", message.requester);

    runOffMainThread<FoundCredentials>(
        [pending, assertion = *request]
        {
            return findCredentialsForAssertion(
                assertion, pending.requester.origin, *pending.keyStore);
        },
        [this, pending, assertion = *request](const FoundCredentials& found)
        {
            if (std::holds_alternative<PasskeyError>(found))
            {
                auto& error = std::get<PasskeyError>(found);
                finishRequest(pending, buildPasskeyErrorReply(assertion.requestId, error));
                return;
            }

            continueAssertion(
                pending, assertion, std::get<std::vector<PasskeyCredential>>(found));
        });
}

// The chooser runs on the main thread; the page may have aborted meanwhile.
void PasskeyRequests::continueAssertion(const PendingRequest& pending,
                                        const PasskeyAssertionRequest& request,
                                        const std::vector<PasskeyCredential>& credentials)
{
    if (!isStillPending(pending))
        return;

    auto credential = chooseCredential(credentials);

    if (!credential)
    {
        auto reply = buildPasskeyErrorReply(request.requestId, makeNoPasskeyChosenError());
        finishRequest(pending, reply);
        return;
    }

    if (!isStillPending(pending))
        return;

    runOffMainThread<std::string>(
        [pending, request, chosen = *credential]
        {
            return answerPasskeyAssertionRequest(
                request, pending.requester.origin, chosen, *pending.keyStore);
        },
        [this, pending](const std::string& reply) { finishRequest(pending, reply); });
}

void PasskeyRequests::abortRequest(const PageMessage& message)
{
    auto requestId = readPasskeyMessageHeader(message.text).requestId;
    auto isAborted = [&](const PendingRequest& pending)
    {
        return pending.requester.tabId == message.requester.tabId
               && pending.requestId == requestId;
    };

    for (auto& pending: pendingRequests)
    {
        if (isAborted(pending))
            pending.keyStore->cancelUserPrompt();
    }

    std::erase_if(pendingRequests, isAborted);
}

void PasskeyRequests::replyUnreadable(const PageMessage& message)
{
    auto requestId = readPasskeyMessageHeader(message.text).requestId;
    replyToTab(message.requester,
               buildPasskeyErrorReply(requestId, makeUnreadableRequestError()));
}

// A page that navigated starts its request ids again, so one still pending
// under the same id belongs to the page that left: its prompt is closed.
PasskeyRequests::PendingRequest PasskeyRequests::addPendingRequest(const PageMessage& message)
{
    abortRequest(message);

    auto pending = PendingRequest {message.requester,
                                   readPasskeyMessageHeader(message.text).requestId,
                                   PlatformPasskeyStore::createRequestKeyStore()};
    pendingRequests.push_back(pending);
    return pending;
}

bool PasskeyRequests::isStillPending(const PendingRequest& pending) const
{
    return std::ranges::any_of(pendingRequests, isSameRequestAs(pending));
}

void PasskeyRequests::finishRequest(const PendingRequest& pending, const std::string& reply)
{
    if (!isStillPending(pending))
        return;

    std::erase_if(pendingRequests, isSameRequestAs(pending));
    replyToTab(pending.requester, reply);
}

void PasskeyRequests::replyToTab(const Requester& requester, const std::string& reply)
{
    auto view = tabFinder(requester.tabId);

    if (!view || !isURLOnOrigin(view->get().getURL(), requester.origin))
    {
        logPasskeyEvent("passkey.reply-dropped", requester);
        return;
    }

    view->get().evaluateJavaScript("window.__grazerPasskeyReply?.(" + reply + ")");
}
