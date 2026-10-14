<p align="center">
  <img src="Assets/Icon/Icon.png" width="160" alt="Grazer icon: a cow-spotted ringed planet on a black tile">
</p>

<h1 align="center">Grazer</h1>

<p align="center">
  <b>A small, keyboard-first web browser for macOS. Vim keys, a fuzzy palette, real WebKit.</b>
</p>

Grazer is a browser for people who'd rather not reach for the mouse. Pages
render in WebKit (the engine Safari uses), and everything else is a few keys
away: `j`/`k` to scroll, `f` to click a link, `o` to jump anywhere. There's no
tab strip and no address bar. Instead there's one fuzzy palette that searches
your open tabs, saved places and history, and previews tabs live while you
pick.

It's a nerd browser, but a tidy one: a frameless window, a single 38pt chrome
bar with back/forward, and the page from edge to edge.

<!--
  SCREENSHOT PLACEHOLDER: docs/screenshot-palette.png (palette open over a page).
  Not captured yet: the shell that built this README has no Screen Recording
  permission, so `screencapture -l <window id>` fails with "could not create
  image from window". Grant it under System Settings > Privacy & Security >
  Screen & System Audio Recording for the terminal app, then capture with
  `screencapture -o -l <id> docs/screenshot-palette.png`.
-->
> *Screenshot coming soon.*

## Features

- **Vim-style keys on every page.** Scrolling, history, tabs, link hints and
  find-in-page, injected at document start so they work everywhere, including
  Grazer's own error page.
- **One fuzzy palette for everything.** `o`, `t` or `⌘L` opens it. It's an
  fzf-style matcher over open tabs, saved places and history. Moving the
  selection over an open tab switches to it live underneath, and `Esc` takes
  you back to where you started. Anything that isn't a URL becomes a search,
  always one arrow-key away at the bottom of the list.
- **Places.** Star a page with `b` (or `^S` in the palette) to keep it.
  Places that aren't open still show up in the palette, most recently used
  first.
- **Sessions and history.** Tabs are restored on relaunch. History feeds the
  palette as you type, and `^X` forgets a suggestion you don't want.
- **Link hints.** `f` labels every visible clickable thing with home-row
  letters. Type the label to click it (or focus it, for form fields).
- **A control CLI.** The `Grazer` binary doubles as a client for the running
  browser: list tabs, focus one, or open a URL from your shell, scripts or
  launcher.
- **[Rudder](https://github.com/jamierpond/Rudder) integration.** Grazer
  publishes its tabs to Rudder, the fzf-style app/context switcher, so any
  tab is one search away from anywhere on your Mac. Without Rudder running,
  nothing changes.
- **Plays nice with the web.** Popups and `target=_blank` open as new tabs,
  downloads show a toast, plus pinch-zoom, swipe back/forward, element
  fullscreen for video, and `⌘+` / `⌘-` / `⌘0` page zoom. Grazer identifies
  itself as Safari, so sites (Google sign-in especially) treat it like a real
  browser.
- **One window.** Grazer is a single instance. Launching it again just brings
  the existing window to the front.

## Keys

On a page (ignored while you're typing in a field, where `Esc` leaves the field):

| Key       | Action                                   |
|-----------|------------------------------------------|
| `j` / `k` | Scroll down / up                         |
| `h` / `l` | Scroll left / right                      |
| `d` / `u` | Scroll half a page down / up             |
| `gg` / `G`| Jump to top / bottom                     |
| `H` / `L` | Back / forward                           |
| `r`       | Reload                                   |
| `o`, `t`, `⌘L` | Open the palette                    |
| `J` / `K` | Previous / next tab                      |
| `x`       | Close tab                                |
| `b`       | Save or unsave the page as a place       |
| `f`       | Link hints                               |
| `/`       | Find in page (`Enter` to search, `Esc` to close) |
| `n` / `N` | Next / previous match                    |

In the palette:

| Key                         | Action                                     |
|-----------------------------|--------------------------------------------|
| type                        | Fuzzy-filter tabs, places and history      |
| `↓` / `^J` / `^N`           | Next item (open tabs preview live)         |
| `↑` / `^K` / `^P`           | Previous item                              |
| `Enter`                     | Go: open the place, URL or search          |
| `Esc`                       | Cancel and return to the original tab      |
| `^S`                        | Save or unsave the selected item as a place|
| `^X`                        | Close the tab, unsave the place, or forget the history entry |

When the palette opens, the current URL is pre-filled and selected, so
`⌘L`, `⌘C` copies it and typing anything replaces it.

## Control CLI

The same binary talks to the running Grazer over a local IPC channel, without
opening a second window:

```sh
alias grazer=/path/to/Grazer.app/Contents/MacOS/Grazer

grazer --list-open-tabs        # id <TAB> * if active <TAB> title <TAB> url
grazer --focus-tab 3           # bring tab 3 to the front
grazer --open-url github.com   # open a URL (or a search) in a new tab
```

`--list-open-tabs` output is tab-separated, so
`grazer --list-open-tabs | cut -f1` gives ids for `--focus-tab`. Commands exit
non-zero with `grazer: not running` if there's no browser to talk to.

## Building

Requirements:

- macOS 11 or later (`CMAKE_OSX_DEPLOYMENT_TARGET` is 11.0)
- CMake 3.31+, Ninja, Node.js, [just](https://github.com/casey/just)
- Xcode command line tools

```sh
just run     # configure (first time), build, and open build/Grazer.app
just build   # build only
just clean   # delete build/
```

`ctest --test-dir build` runs the unit tests for the browser logic.

> **Heads up: eacp.** Grazer uses WebView APIs (custom user agent, download
> callbacks, gestures, element fullscreen) that haven't landed on
> [eacp](https://github.com/jamierpond/eacp) `main` yet, so a fresh clone
> won't build against it as is. Until they do, point CPM at a local eacp
> checkout that has them:
>
> ```sh
> cmake -G Ninja -B build -DCPM_eacp_SOURCE=$HOME/path/to/eacp
> ```

## How it's built

- **[eacp](https://github.com/jamierpond/eacp)** provides the app shell:
  windows, menus, IPC and a C++ wrapper around `WKWebView`. Every tab is a
  `WKWebView`, and so are the chrome bar and the palette.
- **The palette** is a small React + Vite app in `web/`, rendered in a
  transparent web view over the page. It doesn't do much itself: matching,
  ranking, selection and preview all happen in C++, and the UI draws what
  it's sent over a typed bridge generated from `Source/Types.h`.
- **Browser logic** (URL/omnibox parsing, fuzzy matching, places, history,
  MRU, sessions, the error page) is a plain static library in `Lib/Browser`,
  unit-tested with NanoTest.
- **[emberstore](https://github.com/tamber-inc/emberstore)** persists places,
  history and sessions as JSON documents under
  `~/Library/Application Support/pond/Grazer`.
- **[CPM](https://github.com/cpm-cmake/CPM.cmake)** fetches the dependencies
  (eacp, emberstore, Rudder's client library, NanoTest), so there's nothing
  to install by hand.
- The app icon comes from `Assets/Icon/Source.png`. `just icon` rebuilds the
  macOS `Icon.png` (Apple's 824-on-1024 grid) and the Windows `Icon.ico`
  (near full-bleed tile, plain silhouette at 16–48 px) from it.

## Coming

Passkey support is in progress, waiting on Apple's browser entitlement.
