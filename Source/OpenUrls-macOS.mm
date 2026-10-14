#include "OpenUrls.h"

#import <AppKit/AppKit.h>

#include <utility>

namespace OpenUrls
{
namespace
{
Handler handler;
std::vector<std::string> pending;
bool launched = false;
std::vector<std::function<void()>> launchCallbacks;

void receive(std::string url)
{
    if (handler)
        handler(url);
    else
        pending.push_back(std::move(url));
}
} // namespace
} // namespace OpenUrls

@interface WimUrlEventReceiver : NSObject
@end

@implementation WimUrlEventReceiver

- (void)handleGetUrl:(NSAppleEventDescriptor*)event
      withReplyEvent:(NSAppleEventDescriptor*)reply
{
    auto* url = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];

    if (url.length > 0)
        OpenUrls::receive(url.UTF8String);
}

@end

namespace OpenUrls
{

// AppKit dispatches the URL event that launched the app inside
// finishLaunching, right after the will-finish notification -- so the handler
// goes in on that notification. Any earlier and AppKit's own setup could
// replace it; any later and a cold launch's URL is lost.
void listen()
{
    static auto* receiver = [[WimUrlEventReceiver alloc] init];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationWillFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                  [[NSAppleEventManager sharedAppleEventManager]
                      setEventHandler:receiver
                          andSelector:@selector(handleGetUrl:withReplyEvent:)
                        forEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
                }];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                  launched = true;

                  for (auto& callback: std::exchange(launchCallbacks, {}))
                      callback();
                }];
}

void setHandler(Handler handlerToUse)
{
    handler = std::move(handlerToUse);

    for (auto& url: takePending())
        handler(url);
}

std::vector<std::string> takePending()
{
    return std::exchange(pending, {});
}

void afterLaunch(std::function<void()> callback)
{
    if (launched)
        callback();
    else
        launchCallbacks.push_back(std::move(callback));
}

} // namespace OpenUrls
