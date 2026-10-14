#include "Console.h"

#include <eacp/Core/Utils/WinInclude.h>

#include <cstdio>

namespace Console
{
namespace
{
void reopen(FILE* stream)
{
    auto* reopened = (FILE*) nullptr;
    freopen_s(&reopened, "CONOUT$", "w", stream);
}
} // namespace

// A handle the launcher already gave us (a pipe, a file) means the stream is
// redirected and works as is; only a missing one is pointed at the console.
// The terminal doesn't wait for a windowed app, so its prompt may print before
// Grazer's output does -- the output still lands in that terminal.
void attachToParent()
{
    auto outMissing = GetStdHandle(STD_OUTPUT_HANDLE) == nullptr;
    auto errMissing = GetStdHandle(STD_ERROR_HANDLE) == nullptr;

    if (!outMissing && !errMissing)
        return;

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;

    if (outMissing)
        reopen(stdout);

    if (errMissing)
        reopen(stderr);
}

} // namespace Console
