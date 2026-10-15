#include "AppMenu.h"

#include <eacp/WebView/WebView.h>

namespace AppMenu
{
using namespace eacp::Graphics;

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
        "Zoom In", WebHelpers::zoomInFocusedWebView, commandKey("+")));
    viewMenu.add(MenuItem::withAction(
        "Zoom Out", WebHelpers::zoomOutFocusedWebView, commandKey("-")));
    viewMenu.add(MenuItem::withAction(
        "Actual Size", WebHelpers::resetSizedFocusedWebView, commandKey("0")));
    bar.add(std::move(viewMenu));

    setApplicationMenuBar(bar, window);
}

} // namespace AppMenu
