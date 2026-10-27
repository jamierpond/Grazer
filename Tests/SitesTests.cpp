#include "TestSupport.h"

#include <PageScript.h>
#include <Sites.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tSeedsGmail = test("Sites/an empty store seeds Gmail's exclusions") = []
{
    auto db = testing::freshDatabase("sites-seed");
    auto store = SitesStore {db};

    auto gmail = store.collection.doc("mail.google.com").get();
    check(gmail.has_value());
    check(!gmail->excludeKeybindings.empty());

    auto& excluded = gmail->excludeKeybindings;
    check(std::find(excluded.begin(), excluded.end(), "o") == excluded.end());
};

auto tKeepsUserEntries = test("Sites/a store with entries is not reseeded") = []
{
    auto db = testing::freshDatabase("sites-keep");
    db.collection<SiteSettings>("sites").doc("example.com").set({{"all"}});

    auto store = SitesStore {db};

    check(store.collection.size() == 1);
    check(store.excludedKeybindingsJSON().find("example.com")
          != std::string::npos);
};

auto tSplicesIntoScript = test("Sites/the page script carries the exclusions") = []
{
    auto script = vimiumScript(R"({"example.com":["j"]})");

    check(script.find(R"(= {"example.com":["j"]};)") != std::string::npos);
    check(script.find("__grazerExcludedKeybindings") == std::string::npos);
};
