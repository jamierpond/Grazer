#pragma once

// Where `Grazer --list-open-tabs` and the other CLI modes print. Grazer is a
// windowed app, which on Windows means no console of its own: run from a
// terminal, its stdout and stderr go nowhere unless they were redirected.
namespace Console
{

// Send stdout and stderr to the terminal that started Grazer, when it was
// started from one and they aren't already redirected. Call before printing.
// A no-op where a windowed app already inherits its terminal (macOS).
void attachToParent();

} // namespace Console
