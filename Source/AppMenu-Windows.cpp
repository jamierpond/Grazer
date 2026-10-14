#include "AppMenu.h"

namespace AppMenu
{

// No menu bar: a Win32 one would draw inside Grazer's frameless window, and
// WebView2 already takes Ctrl+C/V/X/A and Ctrl+plus/minus/0 itself.
void install() {}

} // namespace AppMenu
