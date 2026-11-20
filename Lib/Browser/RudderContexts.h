#pragma once

#include "History.h"
#include "Mru.h"
#include "Places.h"

#include <GrazerPalette/RankingMaterials.h>
#include <RudderProtocol.h>

#include <cstdint>
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

// One context per tab.
std::vector<rudder::protocol::Context> buildRudderContexts(const std::vector<RudderTab>& tabs);

// The stores Grazer's palette ordering reads.
struct PaletteStores
{
    MruStore& mru;
    HistoryStore& history;
    PlacesStore& places;
};

// Everything Grazer's palette ordering reads, for Rudder: every window's
// `tabs` as its palette lists them, with the tab pushed to Rudder as focused
// as the current one, the starred places, every recency stamp and the
// history in the order the store lists it.
PaletteRankingMaterials collectPaletteRankingMaterials(std::vector<PaletteTab> tabs,
                                                       const std::vector<rudder::protocol::Context>& pushed,
                                                       const PaletteStores& stores);

// The same, as Rudder's protocol answers a ranking materials request.
rudder::protocol::RankingMaterials collectRudderRankingMaterials(
    std::vector<PaletteTab> tabs,
    const std::vector<rudder::protocol::Context>& pushed,
    const PaletteStores& stores);

// The tab a context id names; nothing for a page's id.
std::optional<std::int64_t> parseRudderTabId(const std::string& id);

// The url a page row Rudder lists from the materials ("url:<url>") opens;
// nothing for a tab's id.
std::optional<std::string> parseRudderPageContextId(const std::string& id);
} // namespace grazer
