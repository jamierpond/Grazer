#include "TestSupport.h"

#include <History.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

namespace
{
grazer::HistoryEntry entryFor(HistoryStore& store, const std::string& url)
{
    for (auto& entry: store.all())
        if (entry.url == url)
            return entry;

    return {};
}
} // namespace

auto tVisitsAccumulate = test("History/repeat visits bump the counter") = []
{
    auto db = testing::freshDatabase("history-visits");
    auto history = HistoryStore {db};

    history.recordVisit("https://example.com", "Example", 1000);
    history.recordVisit("https://example.com/", "Example", 2000);

    auto entry = entryFor(history, "https://example.com/");
    check(entry.visits == 2);
    check(entry.lastVisit == 2000);
};

auto tTitleArrivesLate = test("History/a late title updates the entry") = []
{
    auto db = testing::freshDatabase("history-title");
    auto history = HistoryStore {db};

    history.recordVisit("https://example.com", "", 1000);
    history.updateTitle("https://example.com", "Example Domain");

    check(entryFor(history, "https://example.com").title == "Example Domain");
};

auto tTitleForUnknownURLIsIgnored =
    test("History/titles for unvisited URLs are dropped") = []
{
    auto db = testing::freshDatabase("history-unknown-title");
    auto history = HistoryStore {db};

    history.updateTitle("https://example.com", "Example");

    check(history.all().empty());
};

auto tEmptyTitleKeepsOld =
    test("History/an empty title never clobbers a real one") = []
{
    auto db = testing::freshDatabase("history-empty-title");
    auto history = HistoryStore {db};

    history.recordVisit("https://example.com", "Example", 1000);
    history.recordVisit("https://example.com", "", 2000);

    check(entryFor(history, "https://example.com").title == "Example");
};

auto tForgetDropsTheEntry = test("History/forget drops a recorded visit") = []
{
    auto db = testing::freshDatabase("history-forget");
    auto history = HistoryStore {db};

    history.recordVisit("https://example.com", "", 1000);
    history.forget("https://www.example.com/");

    check(history.all().empty());
};

auto tHistoryPruneKeepsNewest = test("History/prune drops the oldest entries") = []
{
    auto db = testing::freshDatabase("history-prune");
    auto history = HistoryStore {db};

    for (std::size_t i = 0; i < HistoryStore::maxEntries + 5; ++i)
        history.recordVisit("https://site-" + std::to_string(i) + ".com",
                            "Site",
                            (std::int64_t) i + 1);

    history.prune();

    check(history.entries.size() == HistoryStore::maxEntries);
    check(entryFor(history, "https://site-0.com").visits == 0);
};

auto tLoopbackPagesAreNotRecorded =
    test("History/loopback sign-in callbacks are not recorded") = []
{
    auto db = testing::freshDatabase("history-loopback");
    auto history = HistoryStore {db};

    history.recordVisit("http://127.0.0.1:53682/callback?code=abc", "", 1000);
    history.recordVisit("http://localhost:8080/", "", 1000);
    history.recordVisit("http://[::1]:3000/auth", "", 1000);
    history.recordVisit("https://localhost.example.com/", "", 1000);

    check(history.all().size() == 1);
};
