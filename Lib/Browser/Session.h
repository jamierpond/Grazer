#pragma once

#include <Miro/Reflect.h>
#include <emberstore/Emberstore.h>

#include <cstdint>
#include <string>
#include <vector>

namespace grazer
{
// One window's tabs + active index.
struct WindowSession
{
    std::vector<std::string> urls;
    std::int64_t active = 0;

    MIRO_REFLECT(urls, active)
};

// Every open window, in the order they were opened.
struct SessionState
{
    std::vector<WindowSession> windows;

    MIRO_REFLECT(windows)
};

// Drops empty URLs and clamps `active` into range, so a hand-edited or
// corrupt session file can't restore into a broken state.
WindowSession sanitizeWindow(WindowSession window);

// Sanitizes every window and drops the ones left with no tabs.
SessionState sanitizeSession(SessionState state);

// The open windows' tabs, persisted on every tab mutation so a relaunch
// picks up where the last run left off. Atomic database: losing the very last
// change to a crash is fine. A profile from before windows (one "session"
// document of urls + active) loads as a single window.
struct SessionStore
{
    explicit SessionStore(const emberstore::Database& db);

    SessionState load();
    void save(SessionState state);

    emberstore::Document<SessionState> document;
    emberstore::Document<WindowSession> singleWindowDocument;
};
} // namespace grazer
