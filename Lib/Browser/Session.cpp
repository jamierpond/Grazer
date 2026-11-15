#include "Session.h"

#include <algorithm>
#include <utility>

namespace grazer
{
namespace
{
void dropTabsWithEmptyURLs(WindowSession& window)
{
    window.activations.resize(window.urls.size());

    auto kept = WindowSession {};

    for (std::size_t i = 0; i < window.urls.size(); ++i)
    {
        if (!window.urls[i].empty())
        {
            kept.urls.push_back(std::move(window.urls[i]));
            kept.activations.push_back(std::move(window.activations[i]));
        }
    }

    window.urls = std::move(kept.urls);
    window.activations = std::move(kept.activations);
}
} // namespace

WindowSession sanitizeWindow(WindowSession window)
{
    dropTabsWithEmptyURLs(window);

    auto last = std::max((std::int64_t) window.urls.size() - 1, (std::int64_t) 0);
    window.active = std::clamp(window.active, (std::int64_t) 0, last);

    return window;
}

SessionState sanitizeSession(SessionState state)
{
    for (auto& window: state.windows)
        window = sanitizeWindow(std::move(window));

    std::erase_if(state.windows,
                  [](const WindowSession& window) { return window.urls.empty(); });

    return state;
}

SessionStore::SessionStore(const emberstore::Database& db)
    : document(db.document<SessionState>("windows"))
    , singleWindowDocument(db.document<WindowSession>("session"))
{
}

SessionState SessionStore::load()
{
    auto state = sanitizeSession(document.get());

    if (state.windows.empty())
        state = sanitizeSession({{singleWindowDocument.get()}});

    return state;
}

void SessionStore::save(SessionState state)
{
    document.set(sanitizeSession(std::move(state)));
}
} // namespace grazer
