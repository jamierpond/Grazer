#include "BrowserEngine.h"

#include <filesystem>

namespace BrowserEngine
{

void configure(eacp::Graphics::WebView::Options& options)
{
    // The engine tail Safari itself appends. WebKit derives the OS/WebKit
    // prefix, so the UA reads as stock Safari and sites (Google sign-in
    // especially) treat Grazer as a real browser, not an embedded web view.
    options.applicationNameForUserAgent = "Version/26.0 Safari/605.1.15";

    options.backForwardGestures = true;
    options.allowsMagnification = true;
    options.elementFullscreen = true;
}

void reportDownloads(eacp::Graphics::WebView& view,
                     std::function<void(const std::string& message)> notify)
{
    view.onDownloadFinished = [notify](const std::string& path)
    {
        auto name = std::filesystem::path(path).filename().string();
        notify("Downloaded " + name);
    };

    view.onDownloadFailed = [notify](const std::string& error)
    { notify("Download failed: " + error); };
}

} // namespace BrowserEngine
