#include "ReloadFromOrigin.h"

#import <AppKit/AppKit.h>

void reloadFromOrigin(eacp::Graphics::WebView& view)
{
    view.focusContent();

    if (![NSApp sendAction:@selector(reloadFromOrigin:) to:nil from:nil])
        view.reload();
}
