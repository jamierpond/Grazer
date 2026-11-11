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

    // File > Open Location... (Cmd+L): the front window's palette. A menu key
    // equivalent, so it works whatever has focus or the page swallows.
    std::function<void()> openLocation;

    // File > Save Page (Cmd+S): download the front window's page.
    std::function<void()> savePage;

    // File > Close Tab (Cmd+W): the front window's active tab. Windows takes
    // Ctrl+W in the page (see PageScript.h).
    std::function<void()> closeTab;

    // View > Zoom In / Zoom Out / Actual Size zoom this: the front window's
    // page. Not WebView::focused, which can be the chrome bar or palette.
    std::function<eacp::Graphics::WebView*()> activePage;
};

// Install Grazer's menu bar. Call once the window exists.
void install(eacp::Graphics::Window& window, Actions actions);

} // namespace AppMenu
