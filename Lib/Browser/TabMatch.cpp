#include "TabMatch.h"
#include "Url.h"

namespace grazer
{
int findOpenTab(const std::vector<OpenTab>& tabs, const std::string& url)
{
    auto wanted = pageKey(url);

    for (std::size_t i = 0; i < tabs.size(); ++i)
        if (pageKey(tabs[i].url) == wanted)
            return (int) i;

    for (std::size_t i = 0; i < tabs.size(); ++i)
        if (!tabs[i].openedUrl.empty() && pageKey(tabs[i].openedUrl) == wanted)
            return (int) i;

    return -1;
}
} // namespace grazer
