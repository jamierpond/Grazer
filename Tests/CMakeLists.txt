add_executable(GrazerTests
        DownloadTests.cpp
        ErrorPageTests.cpp
        HistoryTests.cpp
        MruTests.cpp
        PaletteKeysTests.cpp
        PasskeyCredentialMetadataTests.cpp
        PasskeyTests.cpp
        PlacesTests.cpp
        RudderContextsTests.cpp
        SessionTests.cpp
        SettingsTests.cpp
        SitesTests.cpp
        TabExpiryTests.cpp
        TabMatchTests.cpp)

target_link_libraries(GrazerTests PRIVATE NanoTest NanoTestMain GrazerBrowser)
nano_discover_tests(GrazerTests)
