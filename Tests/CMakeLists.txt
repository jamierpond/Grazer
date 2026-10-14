add_executable(GrazerTests
        ErrorPageTests.cpp
        FuzzyMatchTests.cpp
        HistoryTests.cpp
        MruTests.cpp
        PaletteTests.cpp
        PlacesTests.cpp
        SessionTests.cpp
        UrlTests.cpp)

target_link_libraries(GrazerTests PRIVATE NanoTest NanoTestMain GrazerBrowser)
nano_discover_tests(GrazerTests)
