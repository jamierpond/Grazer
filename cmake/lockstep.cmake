# lockstep bootstrap: commit this file and load it before project() with
#   set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)
# It downloads the pinned lockstep binary and includes the CMake module it writes.
cmake_minimum_required(VERSION 3.25)

set(LOCKSTEP_VERSION 0.8.0)
set(LOCKSTEP_RELEASE_URL https://github.com/jamierpond/lockstep/releases/download)
set(LOCKSTEP_SHA256_macos-arm64 "35fcf3ca92c13e5b4ad7fedf6d0a06dfad9446c420f0e152200fd6ab327b0c08")
set(LOCKSTEP_SHA256_macos-x64 "35fcf3ca92c13e5b4ad7fedf6d0a06dfad9446c420f0e152200fd6ab327b0c08")
set(LOCKSTEP_SHA256_windows-x64 "5179aeca7f90199d1bb937712510709f1306b98c9ab2595ef72d163f706b4a6d")
set(LOCKSTEP_SHA256_windows-arm64 "1a12d8b7baddbcdc52136c5781bc3e11775b286d967b17f55b2609d36ae1b31b")
set(LOCKSTEP_SHA256_linux-x64 "5a28a88c84ba5b70bc0b7f2829882060dd58f623d67e7f6739641b757d76d62a")
set(LOCKSTEP_SHA256_linux-arm64 "7558dc6271d6b27971cbd546fd3bc87665e44cb397267fe0a306fc0e7caf6210")

set(LOCKSTEP_BOOTSTRAP "${CMAKE_CURRENT_LIST_FILE}")
set(LOCKSTEP_CACHE "$ENV{LOCKSTEP_CACHE}" CACHE PATH "The shared lockstep cache")
if (DEFINED ENV{LOCKSTEP_EXECUTABLE}) # the environment wins over a stale cache entry
    set(LOCKSTEP_EXECUTABLE "$ENV{LOCKSTEP_EXECUTABLE}" CACHE FILEPATH "A lockstep binary to run" FORCE)
endif ()

if (NOT LOCKSTEP_EXECUTABLE)
    string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}" host)
    string(REGEX REPLACE "^darwin" "macos" host "${host}")
    string(REGEX REPLACE "(x86_64|amd64)$" "x64" host "${host}")
    string(REGEX REPLACE "aarch64$" "arm64" host "${host}")
    set(cache "$ENV{HOME}/.cache/lockstep")
    if (LOCKSTEP_CACHE)
        set(cache "${LOCKSTEP_CACHE}")
    elseif (CMAKE_HOST_WIN32)
        set(cache "$ENV{LOCALAPPDATA}/lockstep")
    elseif (NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(cache "$ENV{XDG_CACHE_HOME}/lockstep")
    endif ()
    set(asset "lockstep-${LOCKSTEP_VERSION}-${host}${CMAKE_HOST_EXECUTABLE_SUFFIX}")
    set(LOCKSTEP_EXECUTABLE "${cache}/bin/${LOCKSTEP_VERSION}/${asset}")
    if (NOT EXISTS "${LOCKSTEP_EXECUTABLE}")
        file(DOWNLOAD "${LOCKSTEP_RELEASE_URL}/v${LOCKSTEP_VERSION}/${asset}"
                "${LOCKSTEP_EXECUTABLE}.part" EXPECTED_HASH SHA256=${LOCKSTEP_SHA256_${host}})
        file(CHMOD "${LOCKSTEP_EXECUTABLE}.part" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
        file(RENAME "${LOCKSTEP_EXECUTABLE}.part" "${LOCKSTEP_EXECUTABLE}")
    endif ()
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "LOCKSTEP_EXECUTABLE=${LOCKSTEP_EXECUTABLE}"
        "${LOCKSTEP_EXECUTABLE}" cmake-module
        "--output=${CMAKE_BINARY_DIR}/_lockstep/lockstep-module.cmake" COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_BINARY_DIR}/_lockstep/lockstep-module.cmake")
