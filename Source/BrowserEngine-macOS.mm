#include "BrowserEngine.h"

#include "Download.h"

#include <filesystem>
#include <unordered_set>

#import <objc/runtime.h>

#import <WebKit/WebKit.h>

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

// WebCrypto master key. WebKit asks the navigation delegate for it through
// private selectors; eacp's delegate is built at runtime and Grazer can't add
// methods to it before WebKit caches what it responds to, so the selectors go
// on NSObject. Answering them keeps WebKit off its keychain fallback.

#import <Foundation/Foundation.h>
#import <Security/SecRandom.h>

#include <fcntl.h>
#include <unistd.h>

namespace
{

constexpr auto webCryptoMasterKeyLength = 32;
constexpr auto webCryptoMasterKeyFileName = "webcrypto-master-key";
constexpr auto ownerReadWriteOnly = 0600;

std::string webCryptoMasterKeyPath;

NSData* readWebCryptoMasterKeyFile()
{
    auto* path = [NSString stringWithUTF8String:webCryptoMasterKeyPath.c_str()];
    auto* key = [NSData dataWithContentsOfFile:path];

    if (key.length != webCryptoMasterKeyLength)
        return nil;

    return key;
}

NSData* createWebCryptoMasterKeyFile()
{
    auto* key = [NSMutableData dataWithLength:webCryptoMasterKeyLength];

    if (SecRandomCopyBytes(kSecRandomDefault, webCryptoMasterKeyLength, key.mutableBytes)
        != errSecSuccess)
    {
        return nil;
    }

    auto descriptor = open(webCryptoMasterKeyPath.c_str(),
                           O_WRONLY | O_CREAT | O_EXCL,
                           ownerReadWriteOnly);

    // Lost a race with another tab's first request: use the key it wrote.
    if (descriptor < 0)
        return readWebCryptoMasterKeyFile();

    auto written = write(descriptor, key.bytes, key.length);
    close(descriptor);

    if (written != webCryptoMasterKeyLength)
        return nil;

    return key;
}

NSData* getWebCryptoMasterKey()
{
    if (webCryptoMasterKeyPath.empty())
        return nil;

    if (auto* key = readWebCryptoMasterKeyFile())
        return key;

    return createWebCryptoMasterKeyFile();
}

} // namespace

@interface NSObject (GrazerWebCryptoMasterKey)
@end

@implementation NSObject (GrazerWebCryptoMasterKey)

- (NSData*)_webCryptoMasterKeyForWebView:(id)webView
{
    return getWebCryptoMasterKey();
}

- (void)_webCryptoMasterKeyForWebView:(id)webView
                    completionHandler:(void (^)(NSData*))completionHandler
{
    completionHandler(getWebCryptoMasterKey());
}

@end

// window.outerWidth/outerHeight come from this private UI delegate selector;
// unanswered they read 0, and Google Sheets/Slides take outerWidth/innerWidth
// as the browser zoom and draw their canvases at half resolution.
@interface NSObject (GrazerWindowFrame)
@end

@implementation NSObject (GrazerWindowFrame)

- (void)_webView:(WKWebView*)webView
    getWindowFrameWithCompletionHandler:(void (^)(CGRect))completionHandler
{
    auto* window = webView.window;

    if (window == nil)
    {
        completionHandler([webView convertRect:webView.bounds toView:nil]);
        return;
    }

    completionHandler(window.frame);
}

@end

// eacp's navigation delegate shows any response WebKit can display, so a PDF
// sent as "Content-Disposition: attachment" opened in the tab instead of
// saving. Its class is built at runtime; this wraps its response policy once
// to turn attachments into downloads, which eacp's download delegate saves.
namespace
{

using ResponsePolicyHandler = void (^)(WKNavigationResponsePolicy);
using ResponsePolicyMethod =
    void (*)(id, SEL, WKWebView*, WKNavigationResponse*, ResponsePolicyHandler);

WKWebView* findWebKitView(eacp::Graphics::WebView& view)
{
    auto* container = (__bridge NSView*) view.getHandle();

    for (NSView* subview in container.subviews)
    {
        if ([subview isKindOfClass:[WKWebView class]])
            return (WKWebView*) subview;
    }

    return nil;
}

bool isAttachmentResponse(WKNavigationResponse* response)
{
    if (![response.response isKindOfClass:[NSHTTPURLResponse class]])
        return false;

    auto* httpResponse = (NSHTTPURLResponse*) response.response;
    auto* disposition = [httpResponse valueForHTTPHeaderField:@"Content-Disposition"];

    if (disposition == nil)
        return false;

    return grazer::isAttachmentDisposition(disposition.UTF8String);
}

void downloadAttachmentsFromDelegateClass(Class delegateClass)
{
    static auto wrappedClasses = std::unordered_set<Class> {};

    if (delegateClass == nil || wrappedClasses.contains(delegateClass))
        return;

    auto selector = @selector(webView:decidePolicyForNavigationResponse:decisionHandler:);
    auto* method = class_getInstanceMethod(delegateClass, selector);

    if (method == nullptr)
        return;

    wrappedClasses.insert(delegateClass);
    auto originalImplementation = method_getImplementation(method);
    auto original = reinterpret_cast<ResponsePolicyMethod>(originalImplementation);

    auto wrapper = ^(id delegate,
                     WKWebView* webView,
                     WKNavigationResponse* response,
                     ResponsePolicyHandler handler) {
      if (@available(macOS 11.3, *))
      {
          if (isAttachmentResponse(response))
          {
              handler(WKNavigationResponsePolicyDownload);
              return;
          }
      }

      original(delegate, selector, webView, response, handler);
    };

    method_setImplementation(method, imp_implementationWithBlock(wrapper));
}

} // namespace

namespace BrowserEngine
{

void supplyWebCryptoMasterKeyFrom(const std::string& directory)
{
    webCryptoMasterKeyPath = directory + "/" + webCryptoMasterKeyFileName;
}

// WKWebView ships with picture in picture off on macOS (Safari turns it on);
// only the private preference reaches it, and it applies to a live view.
void allowPictureInPicture(eacp::Graphics::WebView& view)
{
    if (auto* webView = findWebKitView(view))
    {
        [webView.configuration.preferences setValue:@YES
                                             forKey:@"allowsPictureInPictureMediaPlayback"];
    }
}

void saveAttachmentsAsDownloads(eacp::Graphics::WebView& view)
{
    if (auto* webView = findWebKitView(view))
        downloadAttachmentsFromDelegateClass([webView.navigationDelegate class]);
}

void savePage(eacp::Graphics::WebView& view)
{
    auto* webView = findWebKitView(view);

    if (webView == nil || webView.URL == nil)
        return;

    if (@available(macOS 11.3, *))
    {
        auto* delegate = webView.navigationDelegate;
        [webView startDownloadUsingRequest:[NSURLRequest requestWithURL:webView.URL]
                     completionHandler:^(WKDownload* download) {
                       download.delegate = (id<WKDownloadDelegate>) delegate;
                     }];
    }
}

} // namespace BrowserEngine
