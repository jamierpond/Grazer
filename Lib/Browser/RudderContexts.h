#pragma once

#include "History.h"
#include "Mru.h"

#include <RudderProtocol.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace grazer
{
// One tab as Rudder (the app switcher) is told about it.
struct RudderTab
{
    std::int64_t id = 0;
    std::string title;
    std::string url;

    // The front window's active tab.
    bool focused = false;
};

// What Grazer recorded about the user's use of each page: the recency
// stamps and decayed use counts by normalizeURL, the history by pageKey.
struct RecordedPageUse
{
    std::map<std::string, Stamp> stampsByNormalizedUrl;
    std::map<std::string, HistoryEntry> historyByPageKey;
};

RecordedPageUse readRecordedPageUse(MruStore& mru, HistoryStore& history);

// One context per tab, then one per used page (an MruStore stamp) that no tab
// shows, pushed with open=false: picking it in Rudder opens it in a new tab.
// Each carries what Grazer recorded about its use as `usage`, left out where
// Grazer recorded nothing.
std::vector<rudder::protocol::Context> buildRudderContexts(const std::vector<RudderTab>& tabs,
                                                           const RecordedPageUse& recorded);

// The url an unopened page's context id ("url:<url>") opens; nothing for a
// tab's id.
std::optional<std::string> parseRudderPageContextId(const std::string& id);
} // namespace grazer
