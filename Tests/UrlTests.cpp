#include <Url.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tSchemeMeansURL = test("Url/an explicit scheme is always a URL") = []
{
    check(looksLikeURL("https://github.com"));
    check(looksLikeURL("http://x"));
    check(looksLikeURL("app://local/index.html"));
};

auto tDottedTokenIsURL = test("Url/a spaceless dotted token is a URL") = []
{
    check(looksLikeURL("github.com"));
    check(looksLikeURL("news.ycombinator.com/news"));
};

auto tLocalhostIsURL = test("Url/localhost counts as a URL without a dot") = []
{
    check(looksLikeURL("localhost"));
    check(looksLikeURL("localhost:5173/app"));
};

auto tProseIsNotURL = test("Url/anything with spaces is a search") = []
{
    check(!looksLikeURL("how to exit vim"));
    check(!looksLikeURL("weather 94110 today"));
};

auto tBareWordIsNotURL = test("Url/a bare word is a search") = []
{
    check(!looksLikeURL("recipes"));
    check(!looksLikeURL(""));
};

auto tWithSchemeAddsHttps =
    test("Url/withScheme adds https:// when missing, http:// for local hosts") = []
{
    check(withScheme("github.com") == "https://github.com");
    check(withScheme("http://github.com") == "http://github.com");
    check(withScheme("localhost:3000") == "http://localhost:3000");
    check(withScheme("localhost") == "http://localhost");
    check(withScheme("app.localhost:5173/x") == "http://app.localhost:5173/x");
    check(withScheme("192.168.1.20:8080") == "http://192.168.1.20:8080");
    check(withScheme("127.0.0.1/admin") == "http://127.0.0.1/admin");
    check(withScheme("https://localhost:3000") == "https://localhost:3000");
};

auto tLocalDevelopmentHost =
    test("Url/isLocalDevelopmentHost is localhost or an IPv4 address") = []
{
    check(isLocalDevelopmentHost("localhost"));
    check(isLocalDevelopmentHost("LocalHost"));
    check(isLocalDevelopmentHost("api.localhost"));
    check(isLocalDevelopmentHost("10.0.0.1"));
    check(isLocalDevelopmentHost("255.255.255.255"));
    check(!isLocalDevelopmentHost("localhost.com"));
    check(!isLocalDevelopmentHost("mylocalhost"));
    check(!isLocalDevelopmentHost("256.1.1.1"));
    check(!isLocalDevelopmentHost("1.2.3"));
    check(!isLocalDevelopmentHost("1.2.3.4.5"));
    check(!isLocalDevelopmentHost("1..3.4"));
    check(!isLocalDevelopmentHost("1234.1.1.1"));
    check(!isLocalDevelopmentHost("github.com"));
    check(!isLocalDevelopmentHost(""));
};

auto tHostOfTypedAddress = test("Url/getHostOfTypedAddress stops at port, path or query") = []
{
    check(getHostOfTypedAddress("localhost:3000/app") == "localhost");
    check(getHostOfTypedAddress("10.0.0.1/x") == "10.0.0.1");
    check(getHostOfTypedAddress("github.com?q=1") == "github.com");
    check(getHostOfTypedAddress("github.com") == "github.com");
};

auto tPlainHttp = test("Url/isPlainHttpURL is true only for http://") = []
{
    check(isPlainHttpURL("http://neverssl.com/"));
    check(isPlainHttpURL("HTTP://example.com"));
    check(isPlainHttpURL("http://localhost:3000"));
    check(!isPlainHttpURL("https://github.com"));
    check(!isPlainHttpURL("grazer://home"));
    check(!isPlainHttpURL("http:"));
    check(!isPlainHttpURL(""));
};

auto tNavigationURL = test("Url/navigationURL loads URLs and searches prose") = []
{
    check(navigationURL("github.com") == "https://github.com");
    check(navigationURL("localhost:3000") == "http://localhost:3000");
    check(navigationURL("hello world")
          == "https://www.google.com/search?q=hello+world");
};

auto tUrlEncode = test("Url/urlEncode escapes reserved characters") = []
{
    check(urlEncode("a b") == "a+b");
    check(urlEncode("c++ & you") == "c%2B%2B+%26+you");
    check(urlEncode("safe-._~") == "safe-._~");
};

auto tNormalizeStripsNoise =
    test("Url/normalizeURL strips scheme, www. and trailing slashes") = []
{
    check(normalizeURL("https://www.github.com/") == "github.com");
    check(normalizeURL("http://github.com//") == "github.com");
    check(normalizeURL("github.com") == "github.com");
};

auto tNormalizeKeepsPath = test("Url/normalizeURL keeps paths and queries") = []
{
    check(normalizeURL("https://github.com/pulls?q=open")
          == "github.com/pulls?q=open");
};

auto tPageKeyDropsFragment = test("Url/pageKey drops the fragment") = []
{
    check(pageKey("https://mail.google.com/mail/u/1/#inbox")
          == pageKey("https://mail.google.com/mail/u/1/"));
    check(pageKey("https://mail.google.com/mail/u/1/#inbox")
          == "mail.google.com/mail/u/1");
};

auto tPageKeyHostCase = test("Url/pageKey lower-cases the host, not the path") = []
{
    check(pageKey("https://GitHub.com/jamierpond") == "github.com/jamierpond");
    check(pageKey("https://github.com/Tamber-Inc")
          != pageKey("https://github.com/tamber-inc"));
};

auto tPageKeyKeepsQuery = test("Url/pageKey keeps the query") = []
{
    check(pageKey("https://www.google.com/search?q=a")
          != pageKey("https://www.google.com/search?q=b"));
};

auto tHomeResolvesToHomeURL =
    test("Url/grazer:home and grazer://home both open home") = []
{
    check(navigationURL("grazer://home") == homeURL);
    check(navigationURL("grazer:home") == homeURL);
    check(navigationURL("grazer://home/") == homeURL);
    check(isHomeURL(homeURL));
    check(!isHomeURL("https://grazer.home"));
};
