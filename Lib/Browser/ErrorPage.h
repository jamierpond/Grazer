#pragma once

#include <string>

namespace grazer
{
std::string htmlEscape(const std::string& text);

// A self-contained page shown when a navigation fails (DNS, offline, TLS…).
// Both inputs are untrusted and get escaped.
std::string errorPageHTML(const std::string& url, const std::string& error);

// eacp's failure text for a load that was superseded rather than failed (a
// back while one is provisional, a download, a redirect to another app).
bool isCancelledNavigation(const std::string& error);
} // namespace grazer
