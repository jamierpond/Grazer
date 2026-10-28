#include "DefaultBrowser.h"

#import <AppKit/AppKit.h>

namespace DefaultBrowser
{
static NSString* const probeUrlForDefaultBrowser = @"https://example.com";

bool isDefaultBrowser()
{
    auto* probeUrl = [NSURL URLWithString:probeUrlForDefaultBrowser];
    auto* defaultApplicationUrl =
        [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:probeUrl];

    if (defaultApplicationUrl == nil)
        return false;

    auto* resolvedDefaultUrl = defaultApplicationUrl.URLByResolvingSymlinksInPath;
    auto* resolvedGrazerUrl =
        [NSBundle mainBundle].bundleURL.URLByResolvingSymlinksInPath;

    return [resolvedDefaultUrl.path isEqualToString:resolvedGrazerUrl.path];
}

// macOS asks the user to confirm each scheme; an app can't switch silently.
void askToBecomeDefaultBrowser()
{
    if (@available(macOS 12, *))
    {
        auto* workspace = [NSWorkspace sharedWorkspace];
        auto* grazerBundleUrl = [NSBundle mainBundle].bundleURL;

        for (NSString* scheme in @[ @"https", @"http" ])
        {
            [workspace setDefaultApplicationAtURL:grazerBundleUrl
                             toOpenURLsWithScheme:scheme
                                completionHandler:^(NSError*) {}];
        }
    }
}

bool hasUserAcceptedDefaultBrowserOffer()
{
    auto* alert = [[NSAlert alloc] init];
    alert.messageText = @"Make Grazer your default browser?";
    [alert addButtonWithTitle:@"Make Default"];
    [alert addButtonWithTitle:@"Not Now"];

    [NSApp activateIgnoringOtherApps:YES];
    return [alert runModal] == NSAlertFirstButtonReturn;
}
} // namespace DefaultBrowser
