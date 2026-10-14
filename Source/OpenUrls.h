#pragma once

#include <functional>
#include <string>
#include <vector>

// URLs the OS hands Grazer to open. On macOS: `open -b audio.pond.grazer <url>`,
// a link clicked in another app once Grazer is the default browser, NSWorkspace
// openURLs:withApplicationAtURL: (Rudder favourites) -- kAEGetURL Apple events
// that arrive on a cold launch before any window exists, so they queue until
// the app sets a handler. On Windows: the URLs on the command line, which is
// how the shell launches the default browser (`Grazer.exe "%1"`).
namespace OpenUrls
{

using Handler = std::function<void(const std::string& url)>;

// Start receiving URLs. Call in main(), before Apps::run, so the URL that
// launched the app isn't missed.
void listen();

// Deliver every queued URL to `handler` now, and later ones as they arrive.
// Main thread only.
void setHandler(Handler handler);

// The URLs received so far with no handler set, removing them from the queue.
std::vector<std::string> takePending();

// Run `callback` once launching is done -- by then any URL the app was
// launched with has arrived. Eacp's first loop ticks can run before that.
void afterLaunch(std::function<void()> callback);

} // namespace OpenUrls
