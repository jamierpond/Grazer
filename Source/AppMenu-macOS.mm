#include "AppMenu.h"

#include <eacp/WebView/WebView.h>

namespace AppMenu
{

// The Edit menu is load-bearing: macOS only delivers Cmd+X/C/V/A to the
// focused view (the palette input, page fields) by matching them against
// the menu bar -- without it, paste is dead everywhere.
void install()
{
    using namespace eacp::Graphics;

    auto bar = MenuBar {};
    bar.add(standardApplicationMenu("Grazer"));
    bar.add(standardEditMenu());

    auto viewMenu = Menu {"View"};
    viewMenu.add(MenuItem::withAction(
        "Zoom In", WebHelpers::zoomInFocusedWebView, commandKey("+")));
    viewMenu.add(MenuItem::withAction(
        "Zoom Out", WebHelpers::zoomOutFocusedWebView, commandKey("-")));
    viewMenu.add(MenuItem::withAction(
        "Actual Size", WebHelpers::resetSizedFocusedWebView, commandKey("0")));
    bar.add(std::move(viewMenu));

    setApplicationMenuBar(bar);
}

} // namespace AppMenu
