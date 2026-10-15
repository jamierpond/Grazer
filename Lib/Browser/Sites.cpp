#include "Sites.h"

namespace grazer
{
SitesStore::SitesStore(const emberstore::Database& db)
    : collection(db.collection<SiteSettings>("sites"))
{
    if (collection.empty())
        seedDefaults();
}

std::string SitesStore::excludedKeybindingsJSON()
{
    auto excluded = std::map<std::string, std::vector<std::string>> {};

    for (auto& host: collection.ids())
        excluded[host] = collection.doc(host).get()->excludeKeybindings;

    return Miro::toJSONString(excluded);
}

void SitesStore::seedDefaults()
{
    // Every binding Gmail has a shortcut on: j/k/o/u/x/r/f/n/N/l/b/d, and g
    // for its g-then-i style jumps.
    collection.doc("mail.google.com")
        .set({{"j", "k", "o", "u", "x", "r", "f", "n", "N", "l", "b", "d", "g"}});
}
} // namespace grazer
