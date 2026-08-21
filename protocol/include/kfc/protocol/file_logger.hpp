#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace kfc::protocol {

/// How much a line matters; decides whether it is written at all and whether
/// it is pushed to disk immediately. Ordered -- a logger set to Info writes
/// Info, Warning and Error, and drops Debug.
/// Default size at which FileLogger rotates.
inline constexpr std::uintmax_t kDefaultMaxLogBytes = 50 * 1024 * 1024;

enum class LogLevel {
    /// Every protocol message sent/received; buffered rather than flushed
    /// since it's by far the most frequent level.
    Debug,
    /// Things that happened: someone joined, a room opened, a match ended.
    /// One per event, not one per frame.
    Info,
    /// Something is wrong but the server carries on.
    Warning,
    /// Something failed.
    Error,
};

/// Timestamped line-appender used by kfc_server and ServerLink. Rotation is
/// a single-generation size cap (kfc_server.log -> .log.1). Thread-safe (one
/// mutex). Debug is buffered; Info and above flush immediately (rare, and
/// losing the last one before a crash matters). Destructor flushes the rest.
class FileLogger {
public:
    /// Opens (or creates) log_path in append mode. Throws std::runtime_error
    /// if it can't be opened. minimum is the least severe level written;
    /// max_bytes is the size past which the file rotates.
    explicit FileLogger(const std::filesystem::path& log_path, LogLevel minimum = LogLevel::Debug,
                        std::uintmax_t max_bytes = kDefaultMaxLogBytes);

    /// Flushes anything still buffered.
    ~FileLogger();

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    /// Appends one line at Info, prefixed with a wall-clock timestamp.
    void log(const std::string& line);

    /// Appends one line at the given level. Dropped without being written if
    /// level is below the configured minimum.
    void log(LogLevel level, const std::string& line);

    /// Whether a line at this level would be written -- lets a call site skip
    /// building the line (encoding, redacting) when it wouldn't be written.
    [[nodiscard]] bool enabled(LogLevel level) const { return level >= minimum_; }

    /// Pushes anything buffered to disk. Called automatically on any line
    /// above Debug and on destruction.
    void flush();

    /// The level that name spells ("debug", "info", "warning", "error", any
    /// case), or std::nullopt if it spells none of them.
    [[nodiscard]] static std::optional<LogLevel> level_from_name(std::string_view name);

private:
    // Closes, renames the current file to log_path_ + ".1" (replacing any
    // previous one), and reopens a fresh empty file. Called with mutex_ held.
    void rotate();

    LogLevel minimum_;
    std::filesystem::path log_path_;
    std::uintmax_t max_bytes_;
    std::uintmax_t bytes_written_ = 0;
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace kfc::protocol
