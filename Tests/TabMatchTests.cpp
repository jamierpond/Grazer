#include <TabMatch.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tNoTabs = test("TabMatch/nothing open finds nothing") = []
{
    check(findOpenTab({}, "https://github.com/jamierpond") == -1);
};

auto tSamePage = test("TabMatch/a tab showing the page is found") = []
{
    auto tabs = std::vector<OpenTab> {{"https://www.wikipedia.org/", ""},
                                      {"https://github.com/jamierpond", ""}};

    check(findOpenTab(tabs, "https://github.com/jamierpond/") == 1);
    check(findOpenTab(tabs, "https://github.com/jamierpond/Grazer") == -1);
};

auto tFragment = test("TabMatch/the fragment doesn't matter") = []
{
    auto tabs = std::vector<OpenTab> {{"https://mail.google.com/mail/u/1/", ""}};

    check(findOpenTab(tabs, "https://mail.google.com/mail/u/1/#inbox") == 0);
};

auto tRedirected = test("TabMatch/a tab that redirected from the url is found") = []
{
    auto tabs = std::vector<OpenTab> {
        {"https://calendar.google.com/calendar/u/0/r/week",
         "https://calendar.google.com/calendar/u/0/r"}};

    check(findOpenTab(tabs, "https://calendar.google.com/calendar/u/0/r") == 0);
};

auto tShowingWins = test("TabMatch/a tab showing the page beats one opened for it") = []
{
    auto tabs = std::vector<OpenTab> {
        {"https://github.com/sessions/two-factor/mobile?redirect=true",
         "https://github.com/tamber-inc/tamber-web/pulls"},
        {"https://github.com/tamber-inc/tamber-web/pulls", ""}};

    check(findOpenTab(tabs, "https://github.com/tamber-inc/tamber-web/pulls") == 1);
};

auto tMovedOn = test("TabMatch/a tab that navigated elsewhere isn't found") = []
{
    // openedUrl is cleared once the tab navigates somewhere else.
    auto tabs = std::vector<OpenTab> {{"https://github.com/jamierpond/Grazer", ""}};

    check(findOpenTab(tabs, "https://github.com/jamierpond") == -1);
};
