#pragma once

#include <Miro/Reflect.h>
#include <emberstore/Emberstore.h>

#include <cstdint>
#include <string>

namespace grazer
{
struct Stamp
{
    std::int64_t at = 0;

    // Uses so far, each 1, halving every frecencysearch::usageHalfLifeMs, as of `at`.
    double usage = 0;

    MIRO_REFLECT(at, usage)
};

// Recency and decayed use counts for palette ordering, keyed by normalized URL. Written on
// every real use (palette pick, tab cycle, active-tab navigation), so give it
// an Atomic (not Durable) database -- losing the last stamps to a power cut
// is harmless.
struct MruStore
{
    explicit MruStore(const emberstore::Database& db);

    void touch(const std::string& normalizedUrl, std::int64_t atMs = nowMs());

    std::int64_t lastUsed(const std::string& normalizedUrl);

    // The URL's uses decayed to `atMs`; 0 for one never used.
    double usage(const std::string& normalizedUrl, std::int64_t atMs = nowMs());

    // Drops the oldest stamps once the collection outgrows maxEntries.
    void prune();

    static std::int64_t nowMs();

    static constexpr std::size_t maxEntries = 500;

    emberstore::Collection<Stamp> stamps;
};
} // namespace grazer
