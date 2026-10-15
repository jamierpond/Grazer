#include "Session.h"

#include <algorithm>
#include <utility>

namespace grazer
{
WindowSession sanitizeWindow(WindowSession window)
{
    if (window.lastVisited.size() != window.urls.size())
        window.lastVisited.clear();

    auto urls = std::vector<std::string> {};
    auto lastVisited = std::vector<std::int64_t> {};

    for (std::size_t i = 0; i < window.urls.size(); ++i)
    {
        if (window.urls[i].empty())
            continue;

        urls.push_back(std::move(window.urls[i]));

        if (!window.lastVisited.empty())
            lastVisited.push_back(window.lastVisited[i]);
    }

    window.urls = std::move(urls);
    window.lastVisited = std::move(lastVisited);

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
