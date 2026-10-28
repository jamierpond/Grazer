#include "TestSupport.h"

#include <Settings.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tFreshStoreHasNotDeclined =
    test("Settings/a fresh store has not declined the default browser offer") = []
{
    auto db = testing::freshDatabase("settings-fresh");
    auto store = SettingsStore {db};

    check(!store.hasDeclinedDefaultBrowserOffer());
};

auto tDeclineIsRemembered =
    test("Settings/a declined default browser offer survives a reread") = []
{
    auto db = testing::freshDatabase("settings-declined");
    SettingsStore {db}.recordDeclinedDefaultBrowserOffer();

    auto reread = SettingsStore {db};
    check(reread.hasDeclinedDefaultBrowserOffer());
};
