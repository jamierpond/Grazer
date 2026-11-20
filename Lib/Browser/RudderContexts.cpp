#include "RudderContexts.h"

#include <Miro/Reflection/Serialize.h>

#include <algorithm>

namespace grazer
{
namespace
{
constexpr auto pageContextPrefix = std::string_view {"url:"};

std::map<std::string, Stamp> readStamps(MruStore& mru)
{
    auto stamps = std::map<std::string, Stamp> {};
    auto ids = mru.stamps.ids();
    auto values = mru.stamps.values();

    for (auto index = std::size_t {0}; index < ids.size() && index < values.size(); ++index)
        stamps[ids[index]] = values[index];

    return stamps;
}

rudder::protocol::Context createTabContext(const RudderTab& tab)
{
    auto context = rudder::protocol::Context {};
    context.id = std::to_string(tab.id);
    context.title = tab.title.empty() ? tab.url : tab.title;
    context.subtitle = tab.url;
    context.url = tab.url;
    context.focused = tab.focused;
    return context;
}
} // namespace

std::vector<rudder::protocol::Context> buildRudderContexts(const std::vector<RudderTab>& tabs)
{
    auto contexts = std::vector<rudder::protocol::Context> {};

    for (auto& tab: tabs)
        contexts.push_back(createTabContext(tab));

    return contexts;
}

PaletteRankingMaterials collectPaletteRankingMaterials(std::vector<PaletteTab> tabs,
                                                       const std::vector<rudder::protocol::Context>& pushed,
                                                       const PaletteStores& stores)
{
    for (auto& tab: tabs)
        tab.isCurrent = std::ranges::any_of(pushed,
                                            [&tab](const rudder::protocol::Context& context)
                                            { return context.focused && context.id == std::to_string(tab.tabId); });

    return {std::move(tabs), stores.places.all(), readStamps(stores.mru), stores.history.all()};
}

rudder::protocol::RankingMaterials collectRudderRankingMaterials(
    std::vector<PaletteTab> tabs,
    const std::vector<rudder::protocol::Context>& pushed,
    const PaletteStores& stores)
{
    auto materials = collectPaletteRankingMaterials(std::move(tabs), pushed, stores);
    return {paletteRankingMaterialsName, Miro::toJSON(materials)};
}

std::optional<std::int64_t> parseRudderTabId(const std::string& id)
{
    try
    {
        return std::stoll(id);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

std::optional<std::string> parseRudderPageContextId(const std::string& id)
{
    if (!id.starts_with(pageContextPrefix) || id.size() == pageContextPrefix.size())
        return std::nullopt;

    return id.substr(pageContextPrefix.size());
}
} // namespace grazer
