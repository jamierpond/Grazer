#pragma once

// Grazer as the macOS default browser for http and https.
namespace DefaultBrowser
{
bool isDefaultBrowser();

// Raises macOS's own confirmation, one per scheme.
void askToBecomeDefaultBrowser();

// The launch offer: true when the user chose "Make Default".
bool hasUserAcceptedDefaultBrowserOffer();
} // namespace DefaultBrowser
