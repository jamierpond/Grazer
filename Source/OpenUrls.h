#pragma once

#include <functional>
#include <string>
#include <vector>

// URLs macOS hands Grazer: `open -b audio.pond.grazer <url>`, a link clicked in
// another app once Grazer is the default browser, NSWorkspace openURLs:
// withApplicationAtURL: (Rudder favourites). They arrive as kAEGetURL Apple
// events, and on a cold launch before any window exists -- so they queue
// until the app sets a handler. Elsewhere this is a no-op.
namespace OpenUrls
{

using Handler = std::function<void(const std::string& url)>;

#if defined(__APPLE__)

// Start receiving URL events. Call in main(), before Apps::run, so the event
// that launched the app isn't missed.
void listen();

// Deliver every queued URL to `handler` now, and later ones as they arrive.
// Main thread only.
void setHandler(Handler handler);

// The URLs received so far with no handler set, removing them from the queue.
std::vector<std::string> takePending();

// Run `callback` once launching is done -- by then any URL the app was
// launched with has arrived. Eacp's first loop ticks can run before that.
void afterLaunch(std::function<void()> callback);

#else

inline void listen() {}
inline void setHandler(Handler) {}
inline std::vector<std::string> takePending() { return {}; }
inline void afterLaunch(std::function<void()> callback) { callback(); }

#endif

} // namespace OpenUrls
