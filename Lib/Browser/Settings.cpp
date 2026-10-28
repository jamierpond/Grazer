#include "Settings.h"

namespace grazer
{
SettingsStore::SettingsStore(const emberstore::Database& db)
    : document(db.document<Settings>("settings"))
{
}

bool SettingsStore::hasDeclinedDefaultBrowserOffer()
{
    return document.get().declinedDefaultBrowserOffer;
}

void SettingsStore::recordDeclinedDefaultBrowserOffer()
{
    auto settings = document.get();
    settings.declinedDefaultBrowserOffer = true;
    document.set(settings);
}
} // namespace grazer
