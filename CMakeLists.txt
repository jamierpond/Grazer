cmake_minimum_required(VERSION 3.31)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)

project(Grazer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# eacp's `grazer` branch is develop with the open PRs Grazer needs merged in:
# #146 (browser options), #147 (downloads, external links), #148 (camera/mic
# prompt), #141 (keys reach the focused view), #143
# (GlobalHotKey::isAvailable) and #152 (WebView::focused is the one with
# focus). Move back to develop once they merge. Work
# against a local eacp checkout with:
#   cmake -B build -DLOCKSTEP_eacp_SOURCE=$HOME/projects/eacp
lockstep_add_package(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG grazer
        SYSTEM YES)

# Local persistence (Firestore-flavoured JSON documents). Must come after
# eacp -- emberstore fetches its own copy only when no eacp targets exist.
lockstep_add_package(
        NAME emberstore
        GITHUB_REPOSITORY tamber-inc/emberstore
        GIT_TAG main
        SYSTEM YES)

list(APPEND CMAKE_MODULE_PATH "${eacp_SOURCE_DIR}/CMake")

# Publishes Grazer's tabs to Rudder (the app switcher) as contexts, so a tab is
# one search away from anywhere. Only Rudder's client library is built; Grazer
# behaves exactly as before when Rudder isn't running. Work against a local
# Rudder checkout with:
#   cmake -B build -DLOCKSTEP_Rudder_SOURCE=$HOME/projects/Rudder
lockstep_add_package(
        NAME Rudder
        GITHUB_REPOSITORY jamierpond/Rudder
        GIT_TAG main
        OPTIONS "RUDDER_CLIENT_ONLY ON"
        SYSTEM YES)

if (NOT TARGET NanoTest)
    lockstep_add_package(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main
            SYSTEM YES)
endif ()

# Set after the packages, so only this project's own targets build with -Werror.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Grazer's browser logic (URL/omnibox parsing, places, history, session,
# palette ranking) lives in a plain static library so it stays unit-testable
# without the app shell. Types.h (the palette's row model) is shared with the
# app, hence Source on the include path.
add_library(GrazerBrowser STATIC
        Lib/Browser/Download.cpp
        Lib/Browser/ErrorPage.cpp
        Lib/Browser/History.cpp
        Lib/Browser/Mru.cpp
        Lib/Browser/Palette.cpp
        Lib/Browser/PaletteKeys.cpp
        Lib/Browser/Places.cpp
        Lib/Browser/SessionLog.cpp
        Lib/Browser/Session.cpp
        Lib/Browser/Settings.cpp
        Lib/Browser/Sites.cpp
        Lib/Browser/TabExpiry.cpp
        Lib/Browser/TabMatch.cpp
        Lib/Browser/Url.cpp)

target_include_directories(GrazerBrowser PUBLIC Lib/Browser Source)
target_link_libraries(GrazerBrowser PUBLIC emberstore FrecencySearch SearchKeys)

# eacp's bundle plist template plus http/https URL types, so Grazer can be
# picked as the default browser and gets the URLs macOS opens with it.
set(EACP_MACOS_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macOSBundleInfo.plist.in"
        CACHE INTERNAL "eacp macOS bundle Info.plist template")

add_executable(Grazer Source/Main.cpp Source/LoadingBackdrop.cpp Source/PaletteView.cpp)

# eacp's Text/Font.h ends an `if constexpr` chain with a return MSVC calls
# unreachable (C4702); that warning ignores /external:W0, so it would fail
# PaletteView.cpp under -Werror until eacp fixes the header.
if (MSVC)
    set_source_files_properties(Source/PaletteView.cpp PROPERTIES COMPILE_OPTIONS /wd4702)
endif()

set_target_properties(Grazer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Grazer"
        MACOSX_BUNDLE_GUI_IDENTIFIER audio.pond.grazer
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER audio.pond.grazer)

eacp_set_gui_subsystem(Grazer)
set_default_target_setting(Grazer)

target_link_libraries(Grazer PRIVATE GrazerBrowser eacp-webview eacp-network-rpc eacp-ui eacp-gpu)

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
            Source/DefaultBrowser-macOS.mm
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

if (PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(Tests)
endif ()
