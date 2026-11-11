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

// Save responses sent as "Content-Disposition: attachment" to Downloads,
// even ones the tab could show (a PDF, an image). WebView2 does this itself.
void saveAttachmentsAsDownloads(eacp::Graphics::WebView& view);

// Download the page the tab shows (File > Save Page, Cmd+S) to Downloads,
// reported like any download. Empty on Windows, which has no menu bar.
void savePage(eacp::Graphics::WebView& view);

// Let a tab's videos go picture in picture (Ctrl+P in the page script).
void allowPictureInPicture(eacp::Graphics::WebView& view);

// Hand WebKit the key it wraps sites' stored CryptoKeys with, kept in a file
// in `directory`, so WebKit never reads or writes the "WebCrypto Master Key"
// keychain item (an ad-hoc signed app gets a keychain prompt for it).
void supplyWebCryptoMasterKeyFrom(const std::string& directory);

} // namespace BrowserEngine
