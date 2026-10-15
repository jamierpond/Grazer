#pragma once

#include <eacp/WebView/WebView.h>

#include <functional>
#include <string>

// The application's menu bar, where the platform has one for the app.
namespace AppMenu
{

// What the menu items do in the browser.
struct Actions
{
    // View > Reload Page (Cmd+R) and Reload Ignoring Cache (Shift-Cmd+R).
    std::function<void(bool bypassCache)> reload;

    // Edit > Find... / Find Next / Find Previous (Cmd+F, Cmd+G, Shift-Cmd+G):
    // the find bar call to run in the page -- "open()", "next()", "previous()".
    std::function<void(const std::string& call)> find;

    // File > New Window (Cmd+N). Windows has no menu bar; the page takes
    // Ctrl+N there (see PageScript.h).
    std::function<void()> newWindow;
};

// Install Grazer's menu bar. Call once the window exists.
void install(eacp::Graphics::Window& window, Actions actions);

} // namespace AppMenu
