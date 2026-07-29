#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "kfc/protocol/file_logger.hpp"

using kfc::protocol::FileLogger;
using kfc::protocol::LogLevel;

namespace {

std::filesystem::path fresh_log_path() {
    static int counter = 0;
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("kfc_logger_test_" + std::to_string(counter++) + ".log");
    std::filesystem::remove(path);
    return path;
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

}  // namespace

TEST(FileLoggerTest, WritesTheLineWithATimestampAndItsLevel) {
    std::filesystem::path path = fresh_log_path();
    {
        FileLogger logger(path);
        logger.log("something happened");
    }

    std::string contents = read_all(path);
    EXPECT_NE(contents.find("something happened"), std::string::npos);
    EXPECT_NE(contents.find("[info]"), std::string::npos) << "the bare log() overload is Info";
    // "[YYYY-MM-DD HH:MM:SS.mmm] " -- checked by shape, since the value moves.
    ASSERT_GE(contents.size(), 26u);
    EXPECT_EQ(contents[0], '[');
    EXPECT_EQ(contents[5], '-');
    EXPECT_EQ(contents[14], ':');
    EXPECT_EQ(contents[24], ']');
}

TEST(FileLoggerTest, LinesBelowTheMinimumAreNotWrittenAtAll) {
    std::filesystem::path path = fresh_log_path();
    {
        FileLogger logger(path, LogLevel::Warning);
        logger.log(LogLevel::Debug, "traffic");
        logger.log(LogLevel::Info, "an event");
        logger.log(LogLevel::Warning, "a warning");
        logger.log(LogLevel::Error, "a failure");
    }

    std::string contents = read_all(path);
    EXPECT_EQ(contents.find("traffic"), std::string::npos);
    EXPECT_EQ(contents.find("an event"), std::string::npos);
    EXPECT_NE(contents.find("a warning"), std::string::npos);
    EXPECT_NE(contents.find("a failure"), std::string::npos) << "the minimum is a floor, not an exact match";
}

TEST(FileLoggerTest, EnabledAnswersWithoutWritingAnything) {
    std::filesystem::path path = fresh_log_path();
    FileLogger logger(path, LogLevel::Info);

    EXPECT_FALSE(logger.enabled(LogLevel::Debug));
    EXPECT_TRUE(logger.enabled(LogLevel::Info));
    EXPECT_TRUE(logger.enabled(LogLevel::Error));

    logger.flush();
    EXPECT_TRUE(read_all(path).empty()) << "asking about a level must not log anything";
}

// --- What survives a process that never got to close the file ---

// The point of the level split: a line about something that happened is on disk
// the moment it is written, so a crash cannot swallow it. This is what the old
// flush-every-line behaviour bought, and it has to keep working.
TEST(FileLoggerTest, AnEventIsOnDiskBeforeTheLoggerIsDestroyed) {
    std::filesystem::path path = fresh_log_path();
    FileLogger logger(path);

    logger.log(LogLevel::Info, "the room opened");
    logger.log(LogLevel::Error, "it all went wrong");

    // Read while the logger is still alive and the file still open.
    std::string contents = read_all(path);
    EXPECT_NE(contents.find("the room opened"), std::string::npos);
    EXPECT_NE(contents.find("it all went wrong"), std::string::npos);
}

// And the other half of the trade: a Debug line is *not* pushed to disk as it
// is written, which is what takes the write syscall off the tick thread. (The
// stream still empties its own buffer when that fills up -- batching writes is
// the point; one syscall per line was not.)
TEST(FileLoggerTest, TrafficIsNotPushedToDiskLineByLine) {
    std::filesystem::path path = fresh_log_path();
    FileLogger logger(path);

    logger.log(LogLevel::Debug, "a broadcast nobody needs on disk this instant");

    EXPECT_EQ(read_all(path).find("a broadcast nobody needs on disk this instant"), std::string::npos)
        << "a debug line was flushed as it was written";

    // ...and the next line that *does* matter carries it out with it, so the
    // traffic leading up to an event is on disk by the time the event is.
    logger.log(LogLevel::Warning, "something looks wrong");

    std::string contents = read_all(path);
    EXPECT_NE(contents.find("a broadcast nobody needs on disk this instant"), std::string::npos);
    EXPECT_NE(contents.find("something looks wrong"), std::string::npos);
}

// Buffered must never mean lost: closing the logger puts the tail on disk.
TEST(FileLoggerTest, BufferedTrafficIsFlushedByTheDestructor) {
    std::filesystem::path path = fresh_log_path();
    {
        FileLogger logger(path);
        logger.log(LogLevel::Debug, "the last thing before shutdown");
    }

    EXPECT_NE(read_all(path).find("the last thing before shutdown"), std::string::npos)
        << "buffered is not the same as discarded";
}

TEST(FileLoggerTest, FlushPutsBufferedTrafficOnDiskOnDemand) {
    std::filesystem::path path = fresh_log_path();
    FileLogger logger(path);

    logger.log(LogLevel::Debug, "buffered for now");
    logger.flush();

    EXPECT_NE(read_all(path).find("buffered for now"), std::string::npos);
}

// --- The command-line flag ---

TEST(FileLoggerTest, LevelNamesParseInAnyCaseAndTyposAreRefused) {
    EXPECT_EQ(FileLogger::level_from_name("debug"), LogLevel::Debug);
    EXPECT_EQ(FileLogger::level_from_name("INFO"), LogLevel::Info);
    EXPECT_EQ(FileLogger::level_from_name("Warning"), LogLevel::Warning);
    EXPECT_EQ(FileLogger::level_from_name("error"), LogLevel::Error);

    EXPECT_FALSE(FileLogger::level_from_name("verbose").has_value());
    EXPECT_FALSE(FileLogger::level_from_name("").has_value());
    EXPECT_FALSE(FileLogger::level_from_name("inf").has_value()) << "a prefix is a typo, not a level";
}

TEST(FileLoggerTest, AppendsRatherThanTruncatingAnExistingLog) {
    std::filesystem::path path = fresh_log_path();
    {
        FileLogger logger(path);
        logger.log("from the first run");
    }
    {
        FileLogger logger(path);
        logger.log("from the second run");
    }

    std::string contents = read_all(path);
    EXPECT_NE(contents.find("from the first run"), std::string::npos)
        << "restarting the server must not erase what it logged last time";
    EXPECT_NE(contents.find("from the second run"), std::string::npos);
}

TEST(FileLoggerTest, ThrowsWhenTheLogFileCannotBeOpened) {
    // A directory is never openable as a file, on any platform.
    std::filesystem::path directory = std::filesystem::temp_directory_path() / "kfc_logger_not_a_file";
    std::filesystem::create_directories(directory);

    EXPECT_THROW(FileLogger logger(directory), std::runtime_error);
}

