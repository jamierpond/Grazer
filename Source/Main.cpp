#include "AppMenu.h"
#include "BrowserEngine.h"
#include "Console.h"
#include "Control.h"
#include "ErrorPage.h"
#include "History.h"
#include "Mru.h"
#include "OpenUrls.h"
#include "PageScript.h"
#include "Palette.h"
#include "Places.h"
#include "ReloadFromOrigin.h"
#include "Session.h"
#include "Sites.h"
#include "TabMatch.h"
#include "Types.h"
#include "Url.h"

#include <eacp/Core/Platform/Platform.h>
#include <eacp/Network/IPC/Lock.h>
#include <eacp/Network/IPCRpc/RpcClient.h>
#include <eacp/Network/IPCRpc/RpcServer.h>
#include <eacp/WebView/WebView.h>
#include <eacp/WebView/WebView/JsStringLiteral.h>
#include <emberstore/AppDatabase.h>

#include <RudderClient.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace eacp;
using namespace Graphics;
using namespace grazer;

// `Grazer --snapshot ...` (see main): a throwaway profile, no session, no
// control server, no Rudder -- it must never touch the running browser.
static bool snapshotMode = false;

struct Tab
{
    Tab(std::int64_t idToUse, WebView::Options options)
        : id(idToUse)
        , webView(new WebView(std::move(options)))
    {
    }

    Tab(std::int64_t idToUse, EA::OwningPointer<WebView> adopted)
        : id(idToUse)
        , webView(std::move(adopted))
    {
    }

    WebView& view() { return *webView; }

    std::int64_t id;
    EA::OwningPointer<WebView> webView;
    std::string title;
    std::string url;

    // The URL Grazer was asked to open here, until the tab navigates to any
    // other page -- server redirects don't count, so a tab that went from
    // /r to /r/week (or through a sign-in hop) is still the one asked for.
    std::string openedUrl;

    bool failed = false;
};

struct BrowserWindow;

// What every window shares: the profile's stores, the tab ids, the session,
// and the CLI control plane and Rudder link, which see every window's tabs.
// Each window has its own tabs, palette and chrome (BrowserView).
struct Browser
{
    // Restores the last session's windows (or opens one) and starts serving
    // the control plane and Rudder.
    void start();

    BrowserWindow& openWindow(const WindowSession& state);

    // The user closed `window`: drop it, or quit if it's the last one.
    void closeWindow(BrowserWindow& window);

    // Where menu shortcuts and URLs from outside land: the window last
    // brought forward.
    BrowserWindow* frontWindow();

    void saveSession();

    void setupControl();
    void setupRudder();
    void scheduleRudderPush();
    static std::optional<std::int64_t> rudderTabId(const std::string& contextId);
    void pushTabsToRudder();
    Control::TabList buildTabList();
    std::pair<BrowserWindow*, Tab*> findTab(std::int64_t id);
    Control::Ack focusTabById(std::int64_t id);
    Control::Ack openUrlFromControl(const std::string& text);

    emberstore::Database durable {emberstore::appDataDirectory(
                                      "pond", snapshotMode ? "GrazerSnapshot" : "Grazer"),
                                  emberstore::Durability::Durable};
    emberstore::Database atomic {durable.directory(),
                                 emberstore::Durability::Atomic};
    PlacesStore places {durable};
    SitesStore sites {durable};
    MruStore mru {atomic};
    HistoryStore history {atomic};
    SessionStore session {atomic};
    std::int64_t nextTabId = 0;

    // While the session's windows are being reopened, so the session isn't
    // saved with only the ones made so far.
    bool restoring = false;

    EA::OwnedVector<BrowserWindow> windows;
    BrowserWindow* activeWindow = nullptr;

    std::optional<rudder::RudderClient> rudder;
    std::vector<rudder::protocol::Context> pushedToRudder;
    bool rudderPushPending = false;

    // The CLI control plane. controlServer is last on purpose: it must be torn
    // down before the bridge and api it dispatches into.
    Control::ControlApi controlApi;
    Miro::Bridge controlBridge;
    std::optional<IPC::RpcServer> controlServer;
};

struct BrowserView final : View
{
    explicit BrowserView(Browser& browserToUse)
        : browser(browserToUse)
    {
        api.onSetQuery = [this](const std::string& text) { setPaletteQuery(text); };
        api.onMoveSelection = [this](int delta) { movePaletteSelection(delta); };
        api.onActivate = [this]
        { Threads::callAsync([this] { activatePalette(); }); };
        api.onComplete = [this] { return completePalette(); };
        api.onChoose = [this](const GoItem& item)
        {
            auto chosen = item;
            Threads::callAsync([this, chosen] { chooseItem(chosen); });
        };
        api.onCancel = [this] { Threads::callAsync([this] { cancelPalette(); }); };
        api.onCloseItem = [this](const GoItem& item)
        {
            auto cleared = item;
            Threads::callAsync([this, cleared] { clearItem(cleared); });
        };
        api.onToggleBookmark = [this](const GoItem& item)
        { toggleBookmark(item.title, item.url); };

        transport.getBridge().use(api);

        chromeBar.addScriptMessageHandler(
            "grazerChrome",
            [this](const std::string& command)
            {
                if (command == "toggleMaximize")
                    onToggleMaximize();
                else if (command == "back" && activeTab != nullptr)
                    activeTab->view().goBack();
                else if (command == "forward" && activeTab != nullptr)
                    activeTab->view().goForward();
            });
        chromeBar.loadHTML(chromeBarHtml);

        addChildren({content, chromeBar});
    }

    // Keys back to whatever was taking them -- the palette input, else the
    // page. Raising the window on Windows leaves focus on the window itself,
    // so a raised Grazer took no keys until clicked.
    void focusKeys()
    {
        if (paletteOpen)
            paletteWeb.focusContent();
        else if (activeTab != nullptr)
            activeTab->view().focusContent();
    }

    // Switches to the tab without bringing Grazer forward. An open palette
    // would preview its selection right back over it.
    void showTab(Tab& tab)
    {
        if (paletteOpen)
            hidePalette();

        switchTo(tab);
    }

    Tab* openTabFor(const std::string& url)
    {
        auto open = std::vector<OpenTab> {};

        for (auto& tab: tabs)
            open.push_back({tab->url, tab->openedUrl});

        auto index = findOpenTab(open, url);
        return index < 0 ? nullptr : tabs[index].get();
    }

    // The chrome bar owns the titlebar band (its whole surface is an
    // --eacp-app-region drag handle, traffic lights float inside it); pages
    // lay out below it, never underneath. Tab webviews live inside `content`;
    // the palette is a permanently later sibling, so switching tabs never
    // touches its key focus.
    void resized() override
    {
        auto bounds = getLocalBounds();
        chromeBar.setBounds(bounds.removeFromTop(chromeBarHeight));
        content.setBounds(bounds);

        if (activeTab != nullptr)
            activeTab->view().setBounds(content.getLocalBounds());

        if (paletteOpen)
            paletteWeb.setBounds(getLocalBounds());
    }

    // --- Session ----------------------------------------------------------------

    void restoreSession(const WindowSession& state)
    {
        if (state.urls.empty())
        {
            openTab(homePage);
            return;
        }

        for (auto& url: state.urls)
            createTab(url);

        switchTo(*tabs[(int) state.active]);
        syncPalette();
    }

    WindowSession windowSession()
    {
        auto state = WindowSession {};

        for (auto& tab: tabs)
            state.urls.push_back(tab->url);

        state.active = std::max(tabs.getIndexOfItem(activeTab), 0);
        return state;
    }

    void saveSession() { browser.saveSession(); }

    // --- Tabs -----------------------------------------------------------------

    Tab& createTab(const std::string& url)
    {
        auto& tab = tabs.createNew(nextTabId++, getWebViewOptions());
        tab.view().addUserScript(vimiumScript(sites.excludedKeybindingsJSON()));
        tab.view().addScriptMessageHandler(
            "grazer", [this](const std::string& command) { handleCommand(command); });

        wireTab(tab);

        // Known before any navigation event arrives, so a session save that
        // lands mid-load (or a load that fails outright) keeps the tab.
        tab.url = url;
        tab.openedUrl = url;
        tab.view().loadURL(url);
        return tab;
    }

    void openTab(const std::string& url)
    {
        switchTo(createTab(url));
        syncPalette();
    }

    // A page-initiated popup (target=_blank, window.open) adopted as a tab. It
    // shares the opener's web-process configuration, so the vimium user script
    // and the "grazer" message handler are already installed -- re-adding them
    // would collide on the shared user-content controller.
    void adoptPopup(EA::OwningPointer<WebView> popup)
    {
        auto& tab = tabs.createNew(nextTabId++, std::move(popup));
        wireTab(tab);
        switchTo(tab);
        syncPalette();
    }

    void wireTab(Tab& tab)
    {
        auto* t = &tab;
        auto& view = tab.view();

        // The http guard keeps loadHTML navigations (the error page) from
        // clobbering the tab's real URL or polluting history.
        view.onNavigationStarted = [this, t](const std::string& url)
        {
            if (url.starts_with("http"))
            {
                if (pageKey(url) != pageKey(t->openedUrl))
                    t->openedUrl.clear();

                t->url = url;
                t->failed = false;
                history.recordVisit(url, "", MruStore::nowMs());

                if (t == activeTab)
                    mru.touch(normalizeURL(url));
            }

            syncPalette();
        };

        view.onTitleChanged = [this, t](const std::string& title)
        {
            t->title = title;

            // The error page's own title must not rename the history entry
            // of the URL that failed to load.
            if (!t->failed)
                history.updateTitle(t->url, title);

            syncPalette();
        };

        // The page the navigation landed on: server redirects (a sign-in hop
        // back to the page, /r to /r/week) never start a navigation of their
        // own, so without this the tab keeps the URL it was sent to.
        view.onNavigationFinished = [this, t](const std::string& url)
        {
            if (t == activeTab && !paletteOpen)
                t->view().focusContent();

            updateChromeNav();

            if (url.starts_with("http") && !t->failed && url != t->url)
            {
                t->url = url;
                syncPalette();
            }
        };

        view.onNavigationFailed = [this, t](const std::string& error)
        {
            t->failed = true;
            history.forget(t->url);
            t->view().loadHTML(errorPageHTML(t->url, error));
            syncPalette();
        };

        view.onNewWindowRequested =
            [this](EA::OwningPointer<WebView> popup, const std::string&)
        {
            adoptPopup(std::move(popup));
            return true;
        };

        BrowserEngine::reportDownloads(
            view,
            [this, t](const std::string& message)
            { showToast(activeTab != nullptr ? *activeTab : *t, message); });

        view.onClose = [this, t] { Threads::callAsync([this, t] { closeTab(t); }); };
    }

    void switchTo(Tab& tab)
    {
        if (activeTab == &tab)
            return;

        if (activeTab != nullptr)
            activeTab->view().removeFromParent();

        activeTab = &tab;
        content.addSubview(tab.view());
        tab.view().setBounds(content.getLocalBounds());

        if (!paletteOpen)
            tab.view().focusContent();

        saveSession();
    }

    void closeTab(Tab* tab)
    {
        auto index = tabs.getIndexOfItem(tab);

        if (index < 0)
            return;

        auto wasActive = tab == activeTab;

        if (tab == originTab)
            originTab = nullptr;

        if (wasActive)
        {
            tab->view().removeFromParent();
            activeTab = nullptr;
        }

        tabs.removeAt(index);

        if (tabs.empty())
        {
            openTab(homePage);
            return;
        }

        if (wasActive)
            switchTo(*tabs[std::min(index, tabs.size() - 1)]);

        syncPalette();
    }

    // The palette's ✕ / ctrl-x: remove whatever the row is -- close an open
    // tab, unsave a place, or forget a history suggestion.
    void clearItem(const GoItem& item)
    {
        if (item.isSearch)
            return;

        if (item.tabId >= 0)
        {
            closeTabById(item.tabId);
            return;
        }

        if (item.bookmarked)
            places.toggle(item.title, item.url);
        else
            history.forget(item.url);

        syncPalette();
    }

    void closeTabById(std::int64_t id)
    {
        if (auto* tab = findTab(id))
            closeTab(tab);
    }

    Tab* findTab(std::int64_t id)
    {
        for (auto& tab: tabs)
            if (tab->id == id)
                return tab.get();

        return nullptr;
    }

    void cycleTab(int direction)
    {
        auto index = tabs.getIndexOfItem(activeTab);

        if (index < 0 || tabs.size() < 2)
            return;

        auto next = (index + direction + tabs.size()) % tabs.size();
        mru.touch(normalizeURL(tabs[next]->url));
        switchTo(*tabs[next]);
    }

    // --- Go palette -------------------------------------------------------------

    void showPalette()
    {
        if (paletteOpen)
            return;

        paletteOpen = true;
        originTab = activeTab;
        paletteQuery.clear();
        paletteSelected = 0;
        paletteTouched = false;
        generation++;
        syncPalette();

        addSubview(paletteWeb);
        paletteWeb.setBounds(getLocalBounds());
        paletteWeb.focusContent();
    }

    void hidePalette()
    {
        if (!paletteOpen)
            return;

        paletteOpen = false;
        originTab = nullptr;
        previewDirty = false;
        paletteWeb.removeFromParent();

        if (activeTab != nullptr)
            activeTab->view().focusContent();
    }

    void setPaletteQuery(const std::string& text)
    {
        if (!paletteOpen)
            return;

        paletteQuery = text;
        paletteSelected = 0;
        paletteTouched = true;
        refilterPalette();
        publishResults();
        previewDirty = true;
    }

    // Tab / →: the selected row's URL becomes the query, so it can be edited
    // and Enter opens what's typed. Returns the new input text ("" = no-op).
    std::string completePalette()
    {
        if (!paletteOpen || paletteFiltered.empty())
            return {};

        auto text = completionText(paletteFiltered[(size_t) paletteSelected], paletteQuery);
        setPaletteQuery(text);
        return text;
    }

    void movePaletteSelection(int delta)
    {
        if (!paletteOpen || paletteFiltered.empty())
            return;

        paletteSelected =
            std::clamp(paletteSelected + delta, 0, (int) paletteFiltered.size() - 1);
        paletteTouched = true;
        publishResults();
        previewDirty = false;
        previewSelected();
    }

    // Enter. A touched tab selection was already previewed live; a place or
    // the search row loads now. An untouched palette dismisses without moving.
    void activatePalette()
    {
        if (!paletteOpen)
            return;

        if (paletteTouched && !paletteFiltered.empty())
        {
            openItem(paletteFiltered[(size_t) paletteSelected]);
            return;
        }

        hidePalette();
    }

    void chooseItem(const GoItem& item)
    {
        if (paletteOpen)
            openItem(item);
    }

    void previewSelected()
    {
        if (!paletteOpen || !paletteTouched)
            return;

        if (paletteSelected < (int) paletteFiltered.size())
            previewItem(paletteFiltered[(size_t) paletteSelected]);
    }

    // Typing debounce: setPaletteQuery only marks the preview dirty; this 8 Hz
    // tick applies it, so a fast typist doesn't load a page per keystroke.
    void handlePreviewTick()
    {
        if (!previewDirty)
            return;

        previewDirty = false;
        previewSelected();
    }

    // Selection moved in the palette: switching between OPEN tabs previews
    // live underneath (it's free), so Enter only dismisses. Places that
    // aren't open would cost a page load per selection change, so they only
    // load when actually chosen (Enter / click).
    void previewItem(const GoItem& item)
    {
        if (!paletteOpen || item.tabId < 0)
            return;

        if (auto* tab = findTab(item.tabId))
            switchTo(*tab);
    }

    void openItem(const GoItem& item)
    {
        mru.touch(normalizeURL(item.url));

        if (item.tabId >= 0)
        {
            if (auto* tab = findTab(item.tabId))
                switchTo(*tab);
        }
        else
            openTab(item.url);

        hidePalette();
    }

    void cancelPalette()
    {
        if (originTab != nullptr && originTab != activeTab)
            switchTo(*originTab);

        hidePalette();
    }

    void toggleBookmark(const std::string& title, const std::string& url)
    {
        places.toggle(title, url);
        syncPalette();
    }

    // Rebuild the merged tabs+places list, re-rank it against the current
    // query, push the render model, and keep the on-disk session current.
    // Called on every tab/place mutation so an open palette stays live.
    void syncPalette()
    {
        rebuildPaletteItems();
        refilterPalette();
        publishResults();
        saveSession();
        updateChromeNav();
        browser.scheduleRudderPush();
    }

    void updateChromeNav()
    {
        auto canBack = activeTab != nullptr && activeTab->view().canGoBack();
        auto canForward = activeTab != nullptr && activeTab->view().canGoForward();

        chromeBar.evaluateJavaScript(
            std::string {"window.__grazerNav && window.__grazerNav("}
            + (canBack ? "true" : "false") + "," + (canForward ? "true" : "false")
            + ")");
    }

    void rebuildPaletteItems()
    {
        auto tabItems = std::vector<GoItem> {};

        for (auto& tab: tabs)
        {
            auto item = GoItem {};
            item.tabId = tab->id;
            item.title = tab->title.empty() ? tab->url : tab->title;
            item.url = tab->url;
            item.failed = tab->failed;
            tabItems.push_back(std::move(item));
        }

        paletteAll = mergePaletteItems(std::move(tabItems),
                                       places.all(),
                                       [this](const std::string& url)
                                       { return mru.lastUsed(url); });
    }

    void refilterPalette()
    {
        paletteFiltered = rankPalette(
            paletteAll,
            paletteQuery.empty() ? std::vector<HistoryEntry> {} : history.all(),
            paletteQuery);

        auto last = std::max((int) paletteFiltered.size() - 1, 0);
        paletteSelected = std::clamp(paletteSelected, 0, last);
    }

    void publishResults()
    {
        api.results.publish(
            {paletteFiltered,
             paletteSelected,
             generation,
             activeTab != nullptr ? activeTab->url : std::string {}});
    }

    // --- Page commands ----------------------------------------------------------

    void handleCommand(const std::string& command)
    {
        if (command == "openPalette")
            showPalette();
        else if (command == "toggleBookmark")
        {
            if (activeTab != nullptr)
                toggleBookmark(activeTab->title, activeTab->url);
        }
        else if (command == "closeTab")
            Threads::callAsync([this] { closeTab(activeTab); });
        else if (command == "nextTab")
            cycleTab(1);
        else if (command == "prevTab")
            cycleTab(-1);
        else if (command == "newWindow")
        {
            auto* shared = &browser;
            Threads::callAsync([shared] { shared->openWindow({}); });
        }
    }

    // --- Menu shortcuts ---------------------------------------------------------

    // Cmd+R / Shift-Cmd+R. A failed tab is showing the error page loadHTML put
    // there, so it retries its URL rather than reloading that.
    void reloadActiveTab(bool bypassCache)
    {
        if (activeTab == nullptr || paletteOpen)
            return;

        auto& view = activeTab->view();

        if (activeTab->failed)
            view.loadURL(activeTab->url);
        else if (bypassCache)
            reloadFromOrigin(view);
        else
            view.reload();
    }

    // Cmd+F / Cmd+G / Shift-Cmd+G: drive the page's find bar (see the find
    // section of PageScript.h). Opening it focuses the page first, so its
    // input really takes the keyboard.
    void findInPage(const std::string& call)
    {
        if (activeTab == nullptr || paletteOpen)
            return;

        if (call == "open()")
            activeTab->view().focusContent();

        activeTab->view().evaluateJavaScript("window.__grazerFind?." + call);
    }

    // A page-side notification for events with no chrome of their own
    // (finished downloads, etc.).
    void showToast(Tab& tab, const std::string& message)
    {
        auto script = std::string {"(() => {"}
                      + "const toast = document.createElement('div');"
                      + "toast.textContent = " + jsStringLiteral(message) + ";"
                      + "toast.style.cssText = 'position:fixed;bottom:20px;"
                        "right:20px;z-index:2147483647;background:#222;"
                        "color:#fff;padding:10px 14px;border-radius:8px;"
                        "font:13px -apple-system,sans-serif;"
                        "box-shadow:0 4px 12px rgba(0,0,0,.35)';"
                      + "document.documentElement.appendChild(toast);"
                      + "setTimeout(() => toast.remove(), 4000); })()";

        tab.view().evaluateJavaScript(script);
    }

    static WebView::Options getWebViewOptions()
    {
        auto options = WebView::Options();
        options.statusBar = false;
        BrowserEngine::configure(options);

        // Sites ask before they get the camera or mic, like any browser.
        options.promptForMediaCapture = true;
        return options;
    }

    static WebView::Options paletteOptions()
    {
        auto options = embeddedOptions("GrazerGo");
        options.transparentBackground = true;

        // The palette serves its UI from an embedded scheme the tabs don't
        // register. WebView2 refuses a second scheme set in one user-data
        // folder (ERROR_INVALID_STATE, and the palette never appears), so it
        // gets a folder of its own. No-op on macOS.
        options.userDataFolderSuffix = "Palette";
        return options;
    }

    static constexpr auto homePage = "https://www.wikipedia.org";

    static constexpr auto chromeBarHeight = 38.f;

    // Our own titlebar chrome, FancyWindow-style: one flat surface whose CSS
    // declares the whole bar a window-drag region.
    static constexpr auto chromeBarHtml = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><style>
  html, body { margin: 0; height: 100%; }
  body { background: #131316; --eacp-app-region: drag;
         display: flex; align-items: center; }
  .nav { display: flex; gap: 2px; margin-left: 84px;
         --eacp-app-region: no-drag; }
  .nav button { width: 32px; height: 26px; border: none; border-radius: 6px;
                background: transparent; color: #b9b9c4; font-size: 16px;
                line-height: 1; padding: 0; }
  .nav button:hover { background: #26262e; color: #fff; }
  .nav button:disabled { opacity: .3; background: transparent; }
  .nav button[hidden] { display: none; }
</style></head><body>
<div class="nav">
  <button id="back" title="Back" disabled>&#8592;</button>
  <button id="fwd" title="Forward" hidden>&#8594;</button>
</div>
<script>
  const post = message =>
      window.webkit?.messageHandlers?.grazerChrome?.postMessage(message);

  addEventListener('dblclick', event => {
      if (event.target.tagName !== 'BUTTON')
          post('toggleMaximize');
  });

  back.onclick = () => post('back');
  fwd.onclick = () => post('forward');

  window.__grazerNav = (canBack, canForward) => {
      back.disabled = !canBack;
      fwd.hidden = !canForward;
  };
</script></body></html>)HTML";

    // Bound by BrowserWindow, which owns the Window: double-clicking the chrome
    // bar zooms to fill the workspace, like a native titlebar.
    std::function<void()> onToggleMaximize = [] {};

    Browser& browser;
    Api::GrazerApi api;
    PlacesStore& places = browser.places;
    SitesStore& sites = browser.sites;
    MruStore& mru = browser.mru;
    HistoryStore& history = browser.history;
    View content;
    EA::OwnedVector<Tab> tabs;
    Tab* activeTab = nullptr;
    Tab* originTab = nullptr;
    std::int64_t& nextTabId = browser.nextTabId;
    std::int64_t generation = 0;
    bool paletteOpen = false;
    bool paletteTouched = false;
    bool previewDirty = false;
    int paletteSelected = 0;
    std::string paletteQuery;
    std::vector<GoItem> paletteAll;
    std::vector<GoItem> paletteFiltered;
    WebView chromeBar {getWebViewOptions()};
    WebView paletteWeb {paletteOptions()};
    WebViewBridge transport {paletteWeb};
    Threads::Timer previewTimer {[this] { handlePreviewTick(); }, 8};
};

// One browser window: its tabs, palette and chrome, in a Window. Closing it
// closes its tabs; the app quits with the last window.
struct BrowserWindow
{
    BrowserWindow(Browser& browserToUse, const WindowSession& state)
        : browser(browserToUse)
    {
        view.onToggleMaximize = [this] { window.toggleMaximize(); };
        window.events.onActivationChanged = [this](bool isKey)
        {
            if (isKey && !closed)
                browser.activeWindow = this;
        };

        view.restoreSession(state);
        window.setContentView(view);
        window.toggleMaximize();
    }

    // Raise and activate the window, for when a CLI control command (focus /
    // open / a duplicate launch) brings the browser forward.
    void raise()
    {
        window.toFront();
        view.focusKeys();
    }

    // Frameless chrome: the page runs edge to edge under a hidden, transparent
    // title bar -- only the traffic lights remain, and the green one
    // fullscreens. Starts filling the workspace, like a browser should.
    static WindowOptions getOptions()
    {
        auto options = WindowOptions();

        options.title = "Grazer";
        options.width = 1100;
        options.height = 760;
        options.minWidth = 480;
        options.minHeight = 320;

        options.flags.add(WindowFlags::FullSizeContentView);
        options.showTitle = false;
        options.titlebarTransparent = true;
        options.showTitlebarSeparator = false;
        options.cornerRadius = 12.f;

        // Centers the ~16pt-tall light cluster in the 38pt chrome bar.
        options.trafficLightPosition = Point {17.f, 11.f};

        return options;
    }

    WindowOptions windowOptions()
    {
        auto options = getOptions();
        options.onQuit = [this] { browser.closeWindow(*this); };
        return options;
    }

    Browser& browser;

    // Set once the user closed it: it lingers until the end of the event
    // loop turn, but is no longer part of the session.
    bool closed = false;

    BrowserView view {browser};
    Window window {windowOptions()};
};

void Browser::start()
{
    setupControl();
    setupRudder();

    auto state = session.load();
    restoring = true;

    if (state.windows.empty())
        openWindow({});

    for (auto& window: state.windows)
        openWindow(window);

    restoring = false;
    saveSession();
}

BrowserWindow& Browser::openWindow(const WindowSession& state)
{
    auto& window = windows.createNew(*this, state);
    activeWindow = &window;
    saveSession();
    scheduleRudderPush();
    return window;
}

// The window has closed by the time this runs (on macOS this is its
// windowWillClose), so it's only destroyed once that has unwound.
void Browser::closeWindow(BrowserWindow& window)
{
    auto open = std::count_if(windows.begin(),
                              windows.end(),
                              [](auto& each) { return !each->closed; });

    // The last window's tabs stay in the session for the next launch.
    if (open <= 1)
    {
        Apps::quit();
        return;
    }

    window.closed = true;

    if (activeWindow == &window)
        activeWindow = nullptr;

    saveSession();

    Threads::callAsync(
        [this, closing = &window]
        {
            windows.removeItem(*closing);
            scheduleRudderPush();
        });
}

BrowserWindow* Browser::frontWindow()
{
    if (activeWindow != nullptr)
        return activeWindow;

    for (auto& window: windows)
        if (!window->closed)
            return window.get();

    return nullptr;
}

void Browser::saveSession()
{
    if (snapshotMode || restoring)
        return;

    auto state = SessionState {};

    for (auto& window: windows)
        if (!window->closed)
            state.windows.push_back(window->view.windowSession());

    session.save(std::move(state));
}

// --- CLI control plane ------------------------------------------------------

// Serve the singleton control channel: list / focus / open commands from a
// later `./Grazer --...` invocation land here. Only the lock-holding instance
// reaches this, so the name is ours -- but a control-plane hiccup must
// never take the browser down, hence the swallowed IPC::Error.
void Browser::setupControl()
{
    controlApi.onListTabs = [this] { return buildTabList(); };
    controlApi.onFocusTab = [this](std::int64_t id) { return focusTabById(id); };
    controlApi.onOpenUrl = [this](const std::string& text)
    { return openUrlFromControl(text); };
    controlApi.onReactivate = [this]
    {
        if (auto* window = frontWindow())
            window->raise();

        return Control::Ack {true, ""};
    };

    controlBridge.use(controlApi);

    try
    {
        controlServer.emplace(Control::channelName, controlBridge);
    }
    catch (const IPC::Error&)
    {
    }
}

// --- Rudder -----------------------------------------------------------------

// Grazer's tabs are contexts in Rudder, the app switcher: a tab is a search
// away from anywhere, and picking one lands here. Silent when Rudder isn't
// running.
void Browser::setupRudder()
{
    rudder.emplace(rudder::protocol::Hello {"Grazer", "audio.pond.grazer", 0});
    rudder->onActivate = [this](const std::string& contextId)
    {
        if (auto id = rudderTabId(contextId))
            focusTabById(*id);
    };

    // Rudder previewing a tab: show it, but never activate Grazer or raise
    // the window -- Rudder raises Grazer itself while the row is selected,
    // and a raise from here would land late, over wherever the user went.
    rudder->onPreview = [this](const std::string& contextId)
    {
        if (auto id = rudderTabId(contextId))
            if (auto [window, tab] = findTab(*id); tab != nullptr)
                window->view.showTab(*tab);
    };
}

// Tabs change in bursts (a navigation is a start, a title, a finish), so
// the push waits for the end of the event loop turn and goes once.
void Browser::scheduleRudderPush()
{
    if (rudderPushPending)
        return;

    rudderPushPending = true;
    Threads::callAsync(
        [this]
        {
            rudderPushPending = false;
            pushTabsToRudder();
        });
}

std::optional<std::int64_t> Browser::rudderTabId(const std::string& contextId)
{
    try
    {
        return std::stoll(contextId);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

void Browser::pushTabsToRudder()
{
    if (!rudder)
        return;

    auto contexts = std::vector<rudder::protocol::Context> {};

    for (auto& window: windows)
    {
        for (auto& tab: window->view.tabs)
        {
            auto context = rudder::protocol::Context {};
            context.id = std::to_string(tab->id);
            context.title = tab->title.empty() ? tab->url : tab->title;
            context.subtitle = tab->url;
            context.url = tab->url;
            contexts.push_back(std::move(context));
        }
    }

    if (contexts == pushedToRudder)
        return;

    pushedToRudder = contexts;
    rudder->pushContexts(std::move(contexts));
}

Control::TabList Browser::buildTabList()
{
    auto list = Control::TabList {};

    for (auto& window: windows)
    {
        for (auto& tab: window->view.tabs)
        {
            auto info = Control::TabInfo {};
            info.id = tab->id;
            info.title = tab->title.empty() ? tab->url : tab->title;
            info.url = tab->url;
            info.active = tab.get() == window->view.activeTab;
            info.failed = tab->failed;
            list.tabs.push_back(std::move(info));
        }
    }

    return list;
}

std::pair<BrowserWindow*, Tab*> Browser::findTab(std::int64_t id)
{
    for (auto& window: windows)
        if (auto* tab = window->view.findTab(id))
            return {window.get(), tab};

    return {nullptr, nullptr};
}

Control::Ack Browser::focusTabById(std::int64_t id)
{
    auto [window, tab] = findTab(id);

    if (tab == nullptr)
        return {false, "no open tab with id " + std::to_string(id)};

    mru.touch(normalizeURL(tab->url));
    window->view.showTab(*tab);
    window->raise();
    return {true, ""};
}

Control::Ack Browser::openUrlFromControl(const std::string& text)
{
    if (text.empty())
        return {false, "no url given"};

    // A page already open is switched to rather than loaded again: links
    // from other apps and Rudder's favourites land on the tab they made
    // last time, in whichever window it's in.
    auto url = navigationURL(text);

    for (auto& window: windows)
    {
        if (auto* open = window->view.openTabFor(url))
        {
            mru.touch(normalizeURL(open->url));
            window->view.showTab(*open);
            window->raise();
            return {true, ""};
        }
    }

    auto* window = frontWindow();

    if (window == nullptr)
        return {false, "no open window"};

    // An open palette would preview its selection right back over the
    // new tab.
    window->view.hidePalette();
    window->view.openTab(url);
    window->raise();
    return {true, ""};
}

struct GrazerApp
{
    GrazerApp()
    {
        browser.start();

        // The menu bar is the app's, so its shortcuts act on the front window.
        AppMenu::install(
            browser.frontWindow()->window,
            {.reload =
                 [this](bool bypassCache)
             {
                 if (auto* window = browser.frontWindow())
                     window->view.reloadActiveTab(bypassCache);
             },
             .find =
                 [this](const std::string& call)
             {
                 if (auto* window = browser.frontWindow())
                     window->view.findInPage(call);
             },
             .newWindow = [this] { browser.openWindow({}); }});

        // URLs from the OS (a link opened with Grazer, `open -b audio.pond.grazer`,
        // `Grazer.exe <url>`) open as new tabs, like `--open-url`; ones that
        // launched Grazer waited in the queue for this window.
        OpenUrls::setHandler([this](const std::string& url)
                             { browser.openUrlFromControl(url); });
    }

    Browser browser;
};

// --- Snapshot ------------------------------------------------------------------

// `Grazer --snapshot <out.png> [--palette] [--js <script>] <url>...`: open the URLs as tabs (the
// last one in front) in a 1400x900 window, wait for every page to load and
// settle, optionally open the palette over it, and save the window's own
// rendering -- chrome bar, page and palette web views -- as a PNG. Real Grazer
// pixels for the README on a machine where screencapture has no
// screen-recording grant. Runs beside the installed browser: no lock, no
// control server, and a separate profile, so it never touches its tabs.
struct SnapshotRequest
{
    std::string output;
    std::vector<std::string> urls;
    std::string script;
    bool palette = false;
};

static SnapshotRequest snapshotRequest;

struct SnapshotApp
{
    SnapshotApp()
    {
        window.setContentView(view);

        for (auto& url: snapshotRequest.urls)
            view.openTab(navigationURL(url));
    }

    static WindowOptions getOptions()
    {
        // Borderless, so a tiling window manager leaves its size alone.
        auto options = BrowserWindow::getOptions();
        options.flags = {};
        options.flags.add(WindowFlags::Borderless);
        options.width = 1400;
        options.height = 900;
        options.showInactive = true;
        return options;
    }

    // 4 Hz: wait for every tab to finish loading, then ~3 s for late layout,
    // fonts and images; with --palette, open it and give it ~1.5 s to draw.
    void tick()
    {
        if (capturing)
            return;

        for (auto& tab: view.tabs)
            if (tab->view().isLoading())
                return;

        // --js: run once every tab has loaded, then the settle wait starts
        // over -- e.g. hide a donation banner that isn't Grazer's to show.
        if (!snapshotRequest.script.empty() && !scriptRan)
        {
            scriptRan = true;
            settledTicks = 0;

            for (auto& tab: view.tabs)
                tab->view().evaluateJavaScript(
                    snapshotRequest.script,
                    [](const std::string& result, const std::string& error)
                    {
                        if (!result.empty() || !error.empty())
                            std::fprintf(stderr, "grazer: --js: %s%s\n",
                                         result.c_str(), error.c_str());
                    });
            return;
        }

        if (++settledTicks < 12)
            return;

        if (snapshotRequest.palette && !view.paletteOpen)
        {
            view.showPalette();
            settledTicks = 6;
            return;
        }

        capturing = true;
        view.renderToImageAsync(2.f).then(
            [](Image image)
            {
                if (!image.isValid())
                {
                    std::fputs("grazer: snapshot failed\n", stderr);
                    Apps::quit(1);
                    return;
                }

                image.save(FilePath {snapshotRequest.output});
                Apps::quit(0);
            });
    }

    Browser browser;
    BrowserView view {browser};
    Window window {getOptions()};
    int settledTicks = 0;
    bool scriptRan = false;
    bool capturing = false;
    Threads::Timer timer {[this] { tick(); }, 4};
};

// --- CLI control commands -------------------------------------------------------

// A control command aimed at the running singleton. Reactivate is the internal
// "a duplicate launch lost the lock -- raise the existing window" case; the
// other three come straight from the CLI flags.
enum class ControlKind
{
    ListTabs,
    FocusTab,
    OpenUrl,
    Reactivate
};

struct ControlRequest
{
    ControlKind kind;
    std::int64_t tabId = -1;
    std::string url;
};

namespace
{
// Handed to ControlClientApp, which Apps::run default-constructs and so cannot
// be passed the request directly.
std::optional<ControlRequest> pendingRequest;

[[noreturn]] void usageError(const std::string& message)
{
    std::fprintf(stderr, "grazer: %s\n", message.c_str());
    std::exit(2);
}

std::int64_t parseTabId(const std::string& text)
{
    try
    {
        return std::stoll(text);
    }
    catch (const std::exception&)
    {
        usageError("--focus-tab expects a numeric tab id, got '" + text + "'");
    }
}

// Recognises the control flags; returns nullopt for a plain launch. A known
// flag missing its argument is a hard usage error rather than a silent launch.
std::optional<ControlRequest> parseControlRequest(const std::vector<std::string>& args)
{
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];

        if (arg == "--list-open-tabs")
            return ControlRequest {.kind = ControlKind::ListTabs};

        if (arg == "--focus-tab")
        {
            if (i + 1 >= args.size())
                usageError("--focus-tab needs a tab id");

            return ControlRequest {.kind = ControlKind::FocusTab,
                                   .tabId = parseTabId(args[i + 1])};
        }

        if (arg == "--open-url")
        {
            if (i + 1 >= args.size())
                usageError("--open-url needs a url");

            return ControlRequest {.kind = ControlKind::OpenUrl, .url = args[i + 1]};
        }

        if (arg.starts_with("--"))
            usageError("unknown option '" + arg + "'");
    }

    return std::nullopt;
}
} // namespace

// A windowless app that dials the running instance, issues the pending command
// and quits with the result. It never creates a Window, so `./Grazer --...` acts
// on the existing browser without launching a second one. Missing instance:
// the dial never lands, onDisconnected reports "not running" and exits 1 (a
// duplicate-launch reactivate exits 0 silently -- a plain launch shouldn't
// error just because the peer raced away).
struct ControlClientApp
{
    ControlClientApp()
    {
        // A control command is a quick round trip, not an app -- keep it out
        // of the Dock and the app switcher.
        eacp::Apps::setDockIconVisible(false);

        // A duplicate launch may carry URLs from the OS, which on macOS only
        // land once launching finishes; see forwardUrls.
        client.onConnected = [this]
        {
            connected = true;

            if (pendingRequest->kind == ControlKind::Reactivate)
                OpenUrls::afterLaunch([this] { dispatch(); });
            else
                dispatch();
        };

        client.onDisconnected = [this]
        {
            if (settled)
                return;

            settled = true;
            auto reactivate = pendingRequest->kind == ControlKind::Reactivate;

            if (!connected && !reactivate)
                std::fputs("grazer: not running\n", stderr);

            eacp::Apps::quit(reactivate || connected ? 0 : 1);
        };
    }

    void dispatch()
    {
        const auto& request = *pendingRequest;

        switch (request.kind)
        {
            case ControlKind::ListTabs:
                client.call<Control::TabList>("listTabs")
                    .then([this](Control::TabList list) { printTabs(list); },
                          [this](const std::string& error) { fail(error); });
                break;

            case ControlKind::FocusTab:
                client.call<Control::Ack>("focusTab", Control::TabRef {request.tabId})
                    .then([this](Control::Ack ack) { reportAck(ack); },
                          [this](const std::string& error) { fail(error); });
                break;

            case ControlKind::OpenUrl:
                client.call<Control::Ack>("openUrl", Control::UrlRef {request.url})
                    .then([this](Control::Ack ack) { reportAck(ack); },
                          [this](const std::string& error) { fail(error); });
                break;

            case ControlKind::Reactivate:
                if (auto urls = OpenUrls::takePending(); !urls.empty())
                {
                    forwardUrls(urls);
                    break;
                }

                client.call<Control::Ack>("reactivate")
                    .then([this](Control::Ack) { finish(0); },
                          [this](const std::string&) { finish(0); });
                break;
        }
    }

    // A duplicate launch the OS started with URLs (`open -n`, a second copy of
    // the bundle, `Grazer.exe <url>`): open them in the running Grazer instead
    // of just raising it.
    void forwardUrls(const std::vector<std::string>& urls)
    {
        unanswered = urls.size();

        for (auto& url: urls)
            client.call<Control::Ack>("openUrl", Control::UrlRef {url})
                .then([this](Control::Ack) { answered(); },
                      [this](const std::string&) { answered(); });
    }

    void answered()
    {
        if (--unanswered == 0)
            finish(0);
    }

    // id, active marker, title, url -- tab-separated so `cut -f1` yields the id
    // to hand back to `--focus-tab`.
    void printTabs(const Control::TabList& list)
    {
        for (auto& tab: list.tabs)
            std::printf("%lld\t%s\t%s\t%s\n",
                        (long long) tab.id,
                        tab.active ? "*" : "",
                        tab.title.c_str(),
                        tab.url.c_str());

        finish(0);
    }

    void reportAck(const Control::Ack& ack)
    {
        if (!ack.ok && !ack.message.empty())
            std::fprintf(stderr, "grazer: %s\n", ack.message.c_str());

        finish(ack.ok ? 0 : 1);
    }

    void fail(const std::string& reason)
    {
        if (settled)
            return;

        std::fprintf(stderr, "grazer: %s\n", reason.empty() ? "request failed"
                                                         : reason.c_str());
        finish(1);
    }

    void finish(int code)
    {
        if (settled)
            return;

        settled = true;
        eacp::Apps::quit(code);
    }

    bool connected = false;
    bool settled = false;
    std::size_t unanswered = 0;
    eacp::IPC::RpcClient client {Control::channelName, eacp::Time::MS {4000}};
};

int main(int argc, char* argv[])
{
    using namespace eacp;

    Apps::setCommandLineArgs(argc, argv);

    // Every mode below but a plain launch prints, if only an error.
    if (argc > 1 && std::string {argv[1]}.starts_with("--"))
        Console::attachToParent();

    if (argc >= 4 && std::string {argv[1]} == "--snapshot")
    {
        snapshotMode = true;
        snapshotRequest.output = argv[2];

        for (int i = 3; i < argc; ++i)
        {
            if (std::string {argv[i]} == "--palette")
                snapshotRequest.palette = true;
            else if (std::string {argv[i]} == "--js" && i + 1 < argc)
                snapshotRequest.script = argv[++i];
            else
                snapshotRequest.urls.emplace_back(argv[i]);
        }

        return Apps::run<SnapshotApp>();
    }

    // An explicit control flag never launches a window: talk to the running
    // instance and exit.
    if (auto request = parseControlRequest({argv, argv + argc}))
    {
        pendingRequest = request;
        return Apps::run<ControlClientApp>();
    }

    // Plain launch, maybe carrying URLs from the OS -- start catching them
    // before the loop runs, whichever instance ends up opening them.
    OpenUrls::listen();

    // Hold the per-user singleton for the whole process; the
    // static locals outlive Apps::run, so the lock is released only at exit
    // (or by the kernel on a crash). Losing it means another Grazer already owns
    // the window -- raise that one and go, don't open a second.
    static IPC::Lock instanceLock {Control::lockName};
    static IPC::ScopedLock held {instanceLock};

    if (!held)
    {
        pendingRequest = ControlRequest {.kind = ControlKind::Reactivate};
        return Apps::run<ControlClientApp>();
    }

    return Apps::run<GrazerApp>(argc, argv);
}
