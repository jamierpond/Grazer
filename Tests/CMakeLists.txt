add_executable(GrazerTests
        DownloadTests.cpp
        ErrorPageTests.cpp
        FuzzyMatchTests.cpp
        HistoryTests.cpp
        MruTests.cpp
        PaletteTests.cpp
        PlacesTests.cpp
        SessionLogTests.cpp
        SessionTests.cpp
        SettingsTests.cpp
        SitesTests.cpp
        TabExpiryTests.cpp
        TabMatchTests.cpp
        UrlTests.cpp)

target_link_libraries(GrazerTests PRIVATE NanoTest NanoTestMain GrazerBrowser)
nano_discover_tests(GrazerTests)
