#pragma once

#include <Miro/Reflect.h>
#include <emberstore/Emberstore.h>

#include <cstdint>
#include <string>
#include <vector>

namespace grazer
{
// When one tab was brought to the front, oldest first.
struct TabActivations
{
    std::vector<std::int64_t> atMs;

    MIRO_REFLECT(atMs)
};

// One window's tabs + active index. activations[i] belongs to urls[i].
struct WindowSession
{
    std::vector<std::string> urls;
    std::int64_t active = 0;
    std::vector<TabActivations> activations;

    MIRO_REFLECT(urls, active, activations)
};

// Every open window, in the order they were opened.
struct SessionState
{
    std::vector<WindowSession> windows;
    std::int64_t savedAtMs = 0;

    MIRO_REFLECT(windows, savedAtMs)
};

// Drops empty URLs (with their activations), lines activations up with
// urls, and clamps `active` into range, so a hand-edited or
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
