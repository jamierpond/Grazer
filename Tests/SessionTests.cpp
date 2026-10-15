#include "TestSupport.h"

#include <Session.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tRoundTrips = test("Session/save and load round-trip") = []
{
    auto db = testing::freshDatabase("session-roundtrip");
    auto store = SessionStore {db};

    store.save({{{{"https://a.com", "https://b.com"}, 1}, {{"https://c.com"}, 0}}});

    auto restored = store.load();
    check(restored.windows.size() == 2);
    check(restored.windows[0].urls.size() == 2);
    check(restored.windows[0].urls[1] == "https://b.com");
    check(restored.windows[0].active == 1);
    check(restored.windows[1].urls[0] == "https://c.com");
};

auto tFreshStoreIsEmpty = test("Session/a fresh store loads no windows") = []
{
    auto db = testing::freshDatabase("session-fresh");
    auto store = SessionStore {db};

    check(store.load().windows.empty());
};

auto tSingleWindowProfileLoads =
    test("Session/a profile from before windows loads as one window") = []
{
    auto db = testing::freshDatabase("session-single-window");
    db.document<WindowSession>("session").set({{"https://a.com", "https://b.com"}, 1});

    auto state = SessionStore {db}.load();
    check(state.windows.size() == 1);
    check(state.windows[0].urls.size() == 2);
    check(state.windows[0].active == 1);
};

auto tWindowsWinOverSingleWindow =
    test("Session/saved windows win over the single-window document") = []
{
    auto db = testing::freshDatabase("session-windows-win");
    db.document<WindowSession>("session").set({{"https://old.com"}, 0});

    auto store = SessionStore {db};
    store.save({{{{"https://new.com"}, 0}}});

    auto state = store.load();
    check(state.windows.size() == 1);
    check(state.windows[0].urls[0] == "https://new.com");
};

auto tSanitizeDropsEmptyURLs = test("Session/sanitize drops empty URLs") = []
{
    auto window = sanitizeWindow({{"", "https://a.com", ""}, 0});

    check(window.urls.size() == 1);
    check(window.urls[0] == "https://a.com");
};

auto tSanitizeClampsActive = test("Session/sanitize clamps the active index") = []
{
    check(sanitizeWindow({{"https://a.com"}, 5}).active == 0);
    check(sanitizeWindow({{"https://a.com"}, -3}).active == 0);
    check(sanitizeWindow({{}, 2}).active == 0);
};

auto tSanitizeDropsEmptyWindows = test("Session/sanitize drops windows with no tabs") = []
{
    auto state = sanitizeSession({{{{"", ""}, 0}, {{"https://a.com"}, 3}, {{}, 0}}});

    check(state.windows.size() == 1);
    check(state.windows[0].urls[0] == "https://a.com");
    check(state.windows[0].active == 0);
};
