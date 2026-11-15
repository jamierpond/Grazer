#include <SessionLog.h>

#include <NanoTest/NanoTest.h>

#include <filesystem>
#include <fstream>

using namespace nano;
using namespace grazer;

namespace
{
constexpr std::int64_t sessionStartMs = 1763130000123;
constexpr std::size_t sessionsToKeep = 2;

std::filesystem::path makeFreshLogsDirectory()
{
    auto directory =
        std::filesystem::temp_directory_path() / "GrazerTests" / "SessionLog";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void createEmptyFile(const std::filesystem::path& path)
{
    std::ofstream {path};
}
} // namespace

auto tLogIsNamedBySessionStart =
    test("SessionLog/the log file is named by the session's start in epoch ms") = []
{
    auto path = std::filesystem::path {getSessionLogPath("logs", sessionStartMs)};

    check(path.filename().string() == "1763130000123.log");
    check(path.parent_path().string() == "logs");
};

auto tOldestSessionsAreDeleted =
    test("SessionLog/only the newest sessions are kept") = []
{
    auto toDelete = findSessionLogsToDelete(
        {"1763130000300.log", "1763130000100.log", "1763130000200.log"},
        sessionsToKeep);

    check(toDelete == std::vector<std::string> {"1763130000100.log"});
};

auto tShorterNumberIsOlder =
    test("SessionLog/sessions compare by number, not by text") = []
{
    auto toDelete =
        findSessionLogsToDelete({"900.log", "1000.log", "1100.log"}, sessionsToKeep);

    check(toDelete == std::vector<std::string> {"900.log"});
};

auto tOtherFilesAreLeftAlone =
    test("SessionLog/files that aren't session logs are never deleted") = []
{
    auto toDelete = findSessionLogsToDelete(
        {"notes.txt", "crash.log", ".log", "1.log", "2.log", "3.log"},
        sessionsToKeep);

    check(toDelete == std::vector<std::string> {"1.log"});
};

auto tDeletesFromDisk =
    test("SessionLog/old session logs are deleted from the directory") = []
{
    auto directory = makeFreshLogsDirectory();

    for (auto name: {"100.log", "200.log", "300.log", "keep.txt"})
        createEmptyFile(directory / name);

    deleteOldSessionLogs(directory.string(), sessionsToKeep);

    check(!std::filesystem::exists(directory / "100.log"));
    check(std::filesystem::exists(directory / "200.log"));
    check(std::filesystem::exists(directory / "300.log"));
    check(std::filesystem::exists(directory / "keep.txt"));
};
