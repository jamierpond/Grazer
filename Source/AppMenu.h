#pragma once

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
};

// Install Grazer's menu bar. Call once the window exists.
void install(Actions actions);

} // namespace AppMenu
