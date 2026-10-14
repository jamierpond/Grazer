cmake_minimum_required(VERSION 3.31)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Grazer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

# Work against a local eacp checkout with:
#   cmake -B build -DCPM_eacp_SOURCE=$HOME/projects/eacp
CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY jamierpond/eacp
        GIT_TAG main
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
if (APPLE)
    CPMAddPackage(
            NAME Rudder
            GITHUB_REPOSITORY jamierpond/Rudder
            GIT_TAG main
            OPTIONS "RUDDER_CLIENT_ONLY ON"
    )
endif ()

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
        Lib/Browser/Url.cpp)

target_include_directories(GrazerBrowser PUBLIC Lib/Browser Source)
target_link_libraries(GrazerBrowser PUBLIC emberstore)

# eacp's bundle plist template plus http/https URL types, so Grazer can be
# picked as the default browser and gets the URLs macOS opens with it.
set(EACP_MACOS_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macOSBundleInfo.plist.in"
        CACHE INTERNAL "eacp macOS bundle Info.plist template")

eacp_add_webview_app(Grazer
        SOURCES     Source/Main.cpp
        API         Api::GrazerApi
        API_HEADER  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Types.h
        WEB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY    GrazerGo
        BUNDLE_ID   audio.pond.grazer
        BUNDLE_NAME "Grazer"
        REACT)

target_link_libraries(Grazer PRIVATE GrazerBrowser)

# Takes the URLs macOS hands Grazer (kAEGetURL); see Source/OpenUrls.h.
if (APPLE)
    enable_language(OBJCXX)
    target_sources(Grazer PRIVATE Source/OpenUrls-macOS.mm)
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

if (TARGET RudderClient)
    target_link_libraries(Grazer PRIVATE RudderClient)
    target_compile_definitions(Grazer PRIVATE GRAZER_RUDDER=1)
endif ()

if (NOT TARGET NanoTest)
    CPMAddPackage(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main)
endif ()

enable_testing()
add_subdirectory(Tests)
