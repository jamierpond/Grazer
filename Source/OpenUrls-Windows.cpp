#include "OpenUrls.h"

#include <eacp/Core/Utils/Strings.h>
#include <eacp/Core/Utils/WinInclude.h>

#include <shellapi.h>

#include <utility>

namespace OpenUrls
{
namespace
{
Handler handler;
std::vector<std::string> pending;

void receive(std::string url)
{
    if (handler)
        handler(url);
    else
        pending.push_back(std::move(url));
}
} // namespace

// The shell starts a browser with the URL as an argument (`Grazer.exe "%1"`),
// and a running Grazer is reached by main's single-instance hand-off -- the
// duplicate launch forwards takePending() over the control channel. So every
// URL is on this process's command line, here before the loop runs. Read as
// UTF-16 rather than from argv, which is in the ANSI code page and would
// mangle a non-ASCII URL.
void listen()
{
    auto count = 0;
    auto* args = CommandLineToArgvW(GetCommandLineW(), &count);

    if (args == nullptr)
        return;

    for (auto i = 1; i < count; ++i)
    {
        auto arg = eacp::Strings::narrow(args[i]);

        if (!arg.empty() && !arg.starts_with("--"))
            receive(std::move(arg));
    }

    LocalFree(args);
}

void setHandler(Handler handlerToUse)
{
    handler = std::move(handlerToUse);

    for (auto& url: takePending())
        handler(url);
}

std::vector<std::string> takePending()
{
    return std::exchange(pending, {});
}

// No URL arrives after launch on Windows: listen() has already read them all.
void afterLaunch(std::function<void()> callback)
{
    callback();
}

} // namespace OpenUrls
