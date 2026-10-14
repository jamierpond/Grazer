#pragma once

#include <eacp/WebView/WebView.h>

#include <functional>
#include <string>

// What a tab's web view needs to behave like a browser and not an embedded UI.
// WebKit has to be asked for each piece; WebView2 is a browser's engine and
// does all of it by default, so on Windows these are empty.
namespace BrowserEngine
{

// Browser behaviour for a tab's (and the chrome bar's) options: a user agent
// sites accept for sign-in, history swipes, pinch zoom, element fullscreen.
void configure(eacp::Graphics::WebView::Options& options);

// Report a tab's finished and failed downloads through `notify`, as a line
// for the user ("Downloaded report.pdf").
void reportDownloads(eacp::Graphics::WebView& view,
                     std::function<void(const std::string& message)> notify);

} // namespace BrowserEngine
