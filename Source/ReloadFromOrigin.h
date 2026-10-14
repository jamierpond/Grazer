#pragma once

#include <eacp/WebView/WebView.h>

// Reload the page bypassing the cache (Safari's Shift-Cmd-R). eacp's WebView
// only has reload(); on macOS WKWebView's reloadFromOrigin: is an action
// method, so this focuses the page and sends it down the responder chain.
// Elsewhere it's a plain reload -- WebView2 has no cache-bypassing reload.
#if defined(__APPLE__)

void reloadFromOrigin(eacp::Graphics::WebView& view);

#else

inline void reloadFromOrigin(eacp::Graphics::WebView& view)
{
    view.reload();
}

#endif
