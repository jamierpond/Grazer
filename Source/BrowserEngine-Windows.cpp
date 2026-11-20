#include "BrowserEngine.h"

namespace BrowserEngine
{

// WebView2 sends a full Chromium UA, swipes, zooms and fullscreens by default.
void configure(eacp::Graphics::WebView::Options&) {}

void allowPictureInPicture(eacp::Graphics::WebView&) {}

void saveAttachmentsAsDownloads(eacp::Graphics::WebView&) {}

void savePage(eacp::Graphics::WebView&) {}

bool isPlayingAudio(eacp::Graphics::WebView&)
{
    return false;
}

void supplyWebCryptoMasterKeyFrom(const std::string&) {}

void receivePasskeyMessages(eacp::Graphics::WebView&, PasskeyMessageHandler) {}

// WebView2 runs its own download UI, with its own progress and errors.
void reportDownloads(eacp::Graphics::WebView&,
                     const std::function<void(const std::string&)>&)
{
}

} // namespace BrowserEngine
