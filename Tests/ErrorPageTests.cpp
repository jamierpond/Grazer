#include <ErrorPage.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tEscapesMarkup = test("ErrorPage/htmlEscape neutralizes markup") = []
{
    check(htmlEscape("<script>alert('&')</script>")
          == "&lt;script&gt;alert(&#39;&amp;&#39;)&lt;/script&gt;");
    check(htmlEscape("plain text") == "plain text");
};

auto tPageShowsUrlAndError = test("ErrorPage/the page shows URL and reason") = []
{
    auto html = errorPageHTML("https://example.com", "Server not found");

    check(html.find("https://example.com") != std::string::npos);
    check(html.find("Server not found") != std::string::npos);
};

auto tPageEscapesInputs = test("ErrorPage/untrusted inputs are escaped") = []
{
    auto html = errorPageHTML("https://example.com/<svg>", "<b>boom</b>");

    check(html.find("<svg>") == std::string::npos);
    check(html.find("<b>") == std::string::npos);
};

auto tCancelledLoadsAreNotFailures =
    test("ErrorPage/superseded loads count as cancelled") = []
{
    check(isCancelledNavigation(
        "The operation couldn’t be completed. (NSURLErrorDomain error -999.)"));
    check(isCancelledNavigation("Frame load interrupted"));
    check(isCancelledNavigation(
        "Navigation failed (status=14) for url=https://example.com/"));
};

auto tRealFailuresAreNotCancelled =
    test("ErrorPage/real failures are not cancelled") = []
{
    check(!isCancelledNavigation(
        "A server with the specified hostname could not be found."));
    check(!isCancelledNavigation(
        "The operation couldn’t be completed. (NSURLErrorDomain error -9990.)"));
    check(!isCancelledNavigation(
        "Navigation failed (status=13) for url=https://example.com/"));
    check(!isCancelledNavigation(
        "Navigation failed (status=140) for url=https://example.com/"));
};
