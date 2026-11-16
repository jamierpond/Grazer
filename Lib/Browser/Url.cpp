#include "Url.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>

namespace grazer
{
bool isHomeURL(const std::string& url)
{
    return url == homeURL || url == "grazer://home/" || url == "grazer:home";
}

std::string navigationURL(const std::string& text)
{
    if (isHomeURL(text))
        return homeURL;

    if (looksLikeURL(text))
        return withScheme(text);

    return searchURLFor(text);
}

bool looksLikeURL(const std::string& text)
{
    if (text.empty())
        return false;

    if (text.find("://") != std::string::npos)
        return true;

    if (text.find(' ') != std::string::npos)
        return false;

    return text.find('.') != std::string::npos || text.starts_with("localhost");
}

std::string withScheme(const std::string& url)
{
    if (url.find("://") != std::string::npos)
        return url;

    if (isLocalDevelopmentHost(getHostOfTypedAddress(url)))
        return "http://" + url;

    return "https://" + url;
}

std::string getHostOfTypedAddress(const std::string& text)
{
    return text.substr(0, text.find_first_of(":/?#"));
}

static char toLowerCase(char c)
{
    auto asUnsigned = static_cast<unsigned char>(c);
    auto lowered = std::tolower(asUnsigned);
    return static_cast<char>(lowered);
}

static bool isDigit(char c)
{
    auto asUnsigned = static_cast<unsigned char>(c);
    return std::isdigit(asUnsigned) != 0;
}

static bool isDecimalByte(std::string_view part)
{
    constexpr auto maximumDigits = std::size_t {3};
    constexpr auto maximumByte = 255;
    constexpr auto decimalBase = 10;

    if (part.empty() || part.size() > maximumDigits)
        return false;

    auto value = 0;

    for (auto c: part)
    {
        if (!isDigit(c))
            return false;

        value = value * decimalBase + (c - '0');
    }

    return value <= maximumByte;
}

static bool isIPv4Address(std::string_view host)
{
    constexpr auto partCount = 4;

    for (auto part = 0; part < partCount; ++part)
    {
        auto dot = host.find('.');
        auto isLastPart = part == partCount - 1;

        if (isLastPart != (dot == std::string_view::npos))
            return false;

        if (!isDecimalByte(host.substr(0, dot)))
            return false;

        if (!isLastPart)
            host.remove_prefix(dot + 1);
    }

    return true;
}

bool isLocalDevelopmentHost(const std::string& host)
{
    auto lowered = std::string {};

    for (auto c: host)
        lowered += toLowerCase(c);

    return lowered == "localhost" || lowered.ends_with(".localhost")
           || isIPv4Address(lowered);
}

bool isPlainHttpURL(const std::string& url)
{
    constexpr auto plainScheme = std::string_view {"http://"};

    if (url.size() < plainScheme.size())
        return false;

    for (std::size_t i = 0; i < plainScheme.size(); ++i)
    {
        if (toLowerCase(url[i]) != plainScheme[i])
            return false;
    }

    return true;
}

std::string urlEncode(const std::string& text)
{
    auto encoded = std::string {};

    for (auto c: text)
    {
        if (std::isalnum((unsigned char) c) || c == '-' || c == '_' || c == '.'
            || c == '~')
            encoded += c;
        else if (c == ' ')
            encoded += '+';
        else
        {
            char buffer[4];
            std::snprintf(buffer, sizeof(buffer), "%%%02X", (unsigned char) c);
            encoded += buffer;
        }
    }

    return encoded;
}

std::string searchURLFor(const std::string& query)
{
    return "https://www.google.com/search?q=" + urlEncode(query);
}

std::string normalizeURL(std::string url)
{
    for (auto* prefix: {"https://", "http://"})
    {
        auto view = std::string_view(prefix);

        if (url.starts_with(view))
        {
            url = url.substr(view.size());
            break;
        }
    }

    if (url.starts_with("www."))
        url = url.substr(4);

    while (!url.empty() && url.back() == '/')
        url.pop_back();

    return url;
}

std::string pageKey(const std::string& url)
{
    auto key = normalizeURL(url.substr(0, url.find('#')));
    auto hostEnd = std::min(key.find('/'), key.size());

    for (std::size_t i = 0; i < hostEnd; ++i)
        key[i] = (char) std::tolower((unsigned char) key[i]);

    return key;
}
} // namespace grazer
