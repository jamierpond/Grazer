#pragma once

#include <Miro/Reflect.h>
#include <emberstore/Emberstore.h>

namespace grazer
{
struct Settings
{
    bool declinedDefaultBrowserOffer = false;

    MIRO_REFLECT(declinedDefaultBrowserOffer)
};

// App-wide preferences, one emberstore document.
struct SettingsStore
{
    explicit SettingsStore(const emberstore::Database& db);

    bool hasDeclinedDefaultBrowserOffer();
    void recordDeclinedDefaultBrowserOffer();

    emberstore::Document<Settings> document;
};
} // namespace grazer
