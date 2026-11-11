#include "TabMatch.h"
#include "Url.h"

#include <algorithm>

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

int findMostRecentlyActivatedTab(const std::vector<std::int64_t>& activationStamps,
                                 int fallbackIndex)
{
    if (activationStamps.empty())
        return -1;

    auto best = -1;
    auto bestStamp = std::int64_t {0};

    for (auto index = 0; index < (int) activationStamps.size(); ++index)
    {
        if (activationStamps[index] > bestStamp)
        {
            best = index;
            bestStamp = activationStamps[index];
        }
    }

    if (best >= 0)
        return best;

    auto lastIndex = (int) activationStamps.size() - 1;
    return std::clamp(fallbackIndex, 0, lastIndex);
}
} // namespace grazer
