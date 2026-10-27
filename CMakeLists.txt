cmake_minimum_required(VERSION 3.31)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Grazer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

# eacp's `grazer` branch is develop with the open PRs Grazer needs merged in:
# #146 (browser options), #147 (downloads, external links), #148 (camera/mic
# prompt), #141 (keys reach the focused view), #143
# (GlobalHotKey::isAvailable) and #152 (WebView::focused is the one with
# focus). Move back to develop once they merge. Work
# against a local eacp checkout with:
#   cmake -B build -DCPM_eacp_SOURCE=$HOME/projects/eacp
CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG grazer
)

# Local persistence (Firestore-flavoured JSON documents). Must come after
# eacp -- emberstore fetches its own copy only when no eacp targets exist.
CPMAddPackage(
        NAME emberstore
        GITHUB_REPOSITORY tamber-inc/emberstore
        GIT_TAG main
)

list(APPEND CMAKE_MODULE_PATH "${eacp_SOURCE_DIR}/CMake")

# Publishes Grazer's tabs to Rudder (the app switcher) as contexts, so a tab is
# one search away from anywhere. Only Rudder's client library is built; Grazer
# behaves exactly as before when Rudder isn't running. Work against a local
# Rudder checkout with:
#   cmake -B build -DCPM_Rudder_SOURCE=$HOME/projects/Rudder
CPMAddPackage(
        NAME Rudder
        GITHUB_REPOSITORY jamierpond/Rudder
        GIT_TAG main
        OPTIONS "RUDDER_CLIENT_ONLY ON"
)

# Grazer's browser logic (URL/omnibox parsing, places, history, session,
# palette ranking) lives in a plain static library so it stays unit-testable
# without the app shell. Types.h (the palette wire types) is shared with the
# app / codegen, hence Source on the include path.
add_library(GrazerBrowser STATIC
        Lib/Browser/ErrorPage.cpp
        Lib/Browser/History.cpp
        Lib/Browser/Mru.cpp
        Lib/Browser/Palette.cpp
        Lib/Browser/Places.cpp
        Lib/Browser/Session.cpp
        Lib/Browser/Sites.cpp
        Lib/Browser/TabMatch.cpp
        Lib/Browser/Url.cpp)

target_include_directories(GrazerBrowser PUBLIC Lib/Browser Source)
target_link_libraries(GrazerBrowser PUBLIC emberstore)

# eacp's bundle plist template plus http/https URL types, so Grazer can be
# picked as the default browser and gets the URLs macOS opens with it.
set(EACP_MACOS_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macOSBundleInfo.plist.in"
        CACHE INTERNAL "eacp macOS bundle Info.plist template")

eacp_add_webview_app(Grazer
        SOURCES     Source/Main.cpp Source/LoadingBackdrop.cpp
        API         Api::GrazerApi
        API_HEADER  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Types.h
        WEB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY    GrazerGo
        BUNDLE_ID   audio.pond.grazer
        BUNDLE_NAME "Grazer"
        REACT)

target_link_libraries(Grazer PRIVATE GrazerBrowser eacp-gpu)

# The per-platform halves of the app, each beside the shared header it
# implements: the menu bar (AppMenu), what makes a web view a browser's
# (BrowserEngine), a terminal for the CLI modes (Console), and the URLs the OS
# hands Grazer -- kAEGetURL on macOS, the command line on Windows (OpenUrls).
# Reloading a page past the cache is macOS-only (ReloadFromOrigin).
if (APPLE)
    enable_language(OBJCXX)
    target_sources(Grazer PRIVATE
            Source/AppMenu-macOS.mm
            Source/BrowserEngine-macOS.mm
            Source/Console-macOS.mm
            Source/OpenUrls-macOS.mm
            Source/ReloadFromOrigin-macOS.mm)
elseif (WIN32)
    target_sources(Grazer PRIVATE
            Source/AppMenu-Windows.cpp
            Source/BrowserEngine-Windows.cpp
            Source/Console-Windows.cpp
            Source/OpenUrls-Windows.cpp)
endif ()

# The at-rest icon Finder, the Dock and Explorer draw. Assets/Icon is made by
# `just icon` and committed. macOS builds its .icns from Icon.png; Windows
# takes a prebuilt .ico whose small frames are a plain silhouette, which IMAGE
# (one picture downscaled to every size) can't express.
if (WIN32)
    eacp_set_app_icon(Grazer ICO ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Icon/Icon.ico)
else ()
    eacp_set_app_icon(Grazer IMAGE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Icon/Icon.png)
endif ()

# Passkeys need Apple's browser entitlement + a provisioning profile; off
# until that profile exists. See cmake/GrazerPasskeys.cmake.
include(cmake/GrazerPasskeys.cmake)
grazer_enable_passkeys(Grazer audio.pond.grazer)

target_link_libraries(Grazer PRIVATE RudderClient)

if (NOT TARGET NanoTest)
    CPMAddPackage(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main)
endif ()

enable_testing()
add_subdirectory(Tests)
