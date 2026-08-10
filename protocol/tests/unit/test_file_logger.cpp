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

// --- Rotation ---

TEST(FileLoggerTest, RotatesToDotOneOncePastTheSizeCapAndStartsFresh) {
    std::filesystem::path path = fresh_log_path();
    std::filesystem::path rotated = path;
    rotated += ".1";
    std::filesystem::remove(rotated);

    // Each formatted line here is ~37 bytes -- one alone stays under 50, two
    // together push the file over it.
    FileLogger logger(path, LogLevel::Debug, /*max_bytes=*/50);
    logger.log("one");
    logger.log("two");    // pushes the file over the cap -- rotates after this write
    logger.log("three");  // lands in the fresh file

    std::string old_contents = read_all(rotated);
    EXPECT_NE(old_contents.find("one"), std::string::npos);
    EXPECT_NE(old_contents.find("two"), std::string::npos);

    std::string current_contents = read_all(path);
    EXPECT_EQ(current_contents.find("one"), std::string::npos)
        << "the current file must start fresh after rotation, not carry old content forward";
    EXPECT_EQ(current_contents.find("two"), std::string::npos);
    EXPECT_NE(current_contents.find("three"), std::string::npos);
}

TEST(FileLoggerTest, RotationReplacesAnyPreviousDotOneRatherThanAccumulating) {
    std::filesystem::path path = fresh_log_path();
    std::filesystem::path rotated = path;
    rotated += ".1";
    std::filesystem::remove(rotated);

    FileLogger logger(path, LogLevel::Debug, /*max_bytes=*/50);
    logger.log("gen-one-a");
    logger.log("gen-one-b");  // rotates -- .1 now holds this generation
    logger.log("gen-two-a");
    logger.log("gen-two-b");  // rotates again -- .1 must hold only this one now

    std::string contents = read_all(rotated);
    EXPECT_EQ(contents.find("gen-one"), std::string::npos)
        << "an older generation must not still be sitting in .1 after a later rotation";
    EXPECT_NE(contents.find("gen-two"), std::string::npos);
}

TEST(FileLoggerTest, RestartingAnAlreadyLargeLogCountsItsExistingSizeTowardTheCap) {
    std::filesystem::path path = fresh_log_path();
    std::filesystem::path rotated = path;
    rotated += ".1";
    std::filesystem::remove(rotated);

    // Written directly, not through a FileLogger -- going through one with a
    // small cap would just rotate mid-setup instead of producing the "already
    // large from a previous run" file this test needs to start from.
    {
        std::ofstream preexisting(path);
        preexisting << std::string(1000, 'x');
    }
    ASSERT_GE(std::filesystem::file_size(path), 1000u) << "test setup: the file must already be over the cap";

    // A fresh FileLogger over that file, with a cap of 1000, must rotate on
    // its very first line -- not silently allow another 1000 bytes just
    // because bytes_written_ starts at zero in a new object.
    FileLogger logger(path, LogLevel::Debug, /*max_bytes=*/1000);
    logger.log("first line after restart");

    EXPECT_TRUE(std::filesystem::exists(rotated))
        << "a restart must not forget how large the file already was";
}

TEST(FileLoggerTest, ThrowsWhenTheLogFileCannotBeOpened) {
    // A directory is never openable as a file, on any platform.
    std::filesystem::path directory = std::filesystem::temp_directory_path() / "kfc_logger_not_a_file";
    std::filesystem::create_directories(directory);

    EXPECT_THROW(FileLogger logger(directory), std::runtime_error);
}

