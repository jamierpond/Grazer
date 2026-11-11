#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace grazer
{
// A tab as an outside open (a link from another app, Rudder, --open-url) sees
// it: the page it shows, and the URL Grazer was asked to open in it for as long
// as the tab hasn't navigated anywhere else since.
struct OpenTab
{
    std::string url;
    std::string openedUrl;
};

// The tab an outside open of `url` switches to instead of making another: the
// first showing that page (pageKey), else the first opened for it that only
// redirected since (a sign-in hop, /r landing on /r/week). -1 when none.
int findOpenTab(const std::vector<OpenTab>& tabs, const std::string& url);

// The tab to show after the active one closes: the remaining tab activated most
// recently (highest stamp; 0 = never activated), else `fallbackIndex` when none
// has been activated since launch. -1 when there are no tabs.
int findMostRecentlyActivatedTab(const std::vector<std::int64_t>& activationStamps,
                                 int fallbackIndex);
} // namespace grazer
