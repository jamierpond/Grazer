#pragma once

#include <functional>

// The application's menu bar, where the platform has one for the app.
namespace AppMenu
{

// Install Grazer's menu bar. Call once the window exists. `reload` backs
// View > Reload Page (Cmd+R) and Reload Ignoring Cache (Shift-Cmd+R).
void install(std::function<void(bool bypassCache)> reload);

} // namespace AppMenu
