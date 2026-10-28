#include "AppMenu.h"

#include <eacp/WebView/WebView.h>

#import <AppKit/AppKit.h>

namespace AppMenu
{
using namespace eacp::Graphics;

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

// The Edit menu is load-bearing: macOS only delivers Cmd+X/C/V/A to the
// focused view (the palette input, page fields) by matching them against
// the menu bar -- without it, paste is dead everywhere.
void install(Window& window, Actions actions)
{

    auto bar = MenuBar {};
    bar.add(standardApplicationMenu("Grazer"));

    auto fileMenu = Menu {"File"};
    fileMenu.add(MenuItem::withAction(
        "New Window", [actions] { actions.newWindow(); }, commandKey("n")));
    fileMenu.addSeparator();
    fileMenu.add(MenuItem::withAction("Make Grazer the Default Browser…",
                                      [] { askToBecomeDefaultBrowser(); }));
    bar.add(std::move(fileMenu));

    auto editMenu = standardEditMenu();
    editMenu.addSeparator();
    editMenu.add(MenuItem::withAction(
        "Find…", [actions] { actions.find("open()"); }, commandKey("f")));
    editMenu.add(MenuItem::withAction(
        "Find Next", [actions] { actions.find("next()"); }, commandKey("g")));
    editMenu.add(MenuItem::withAction("Find Previous",
                                      [actions] { actions.find("previous()"); },
                                      commandShiftKey("g")));
    bar.add(std::move(editMenu));

    auto viewMenu = Menu {"View"};
    viewMenu.add(MenuItem::withAction(
        "Reload Page", [actions] { actions.reload(false); }, commandKey("r")));
    viewMenu.add(MenuItem::withAction("Reload Ignoring Cache",
                                      [actions] { actions.reload(true); },
                                      commandShiftKey("r")));
    viewMenu.addSeparator();
    viewMenu.add(MenuItem::withAction(
        "Zoom In",
        [actions]
        {
            if (auto* page = actions.activePage())
                page->zoomIn();
        },
        commandKey("+")));
    viewMenu.add(MenuItem::withAction(
        "Zoom Out",
        [actions]
        {
            if (auto* page = actions.activePage())
                page->zoomOut();
        },
        commandKey("-")));
    viewMenu.add(MenuItem::withAction(
        "Actual Size",
        [actions]
        {
            if (auto* page = actions.activePage())
                page->resetZoom();
        },
        commandKey("0")));
    bar.add(std::move(viewMenu));

    setApplicationMenuBar(bar, window);
}

} // namespace AppMenu
