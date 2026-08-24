#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace kfc::protocol {

/// Default size at which FileLogger rotates.
inline constexpr std::uintmax_t kDefaultMaxLogBytes = 50 * 1024 * 1024;

/// Ordered severity; a logger set to Info writes Info, Warning and Error, and drops Debug.
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

/// Timestamped line-appender used by kfc_server and ServerLink. Rotation is a
/// single-generation size cap (kfc_server.log -> .log.1). Thread-safe.
class FileLogger {
public:
    /// Throws std::runtime_error if log_path can't be opened for append.
    explicit FileLogger(const std::filesystem::path& log_path, LogLevel minimum = LogLevel::Debug,
                        std::uintmax_t max_bytes = kDefaultMaxLogBytes);

    ~FileLogger();

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    /// Appends one line at Info, prefixed with a wall-clock timestamp.
    void log(const std::string& line);

    /// Appends one line at the given level; dropped if below the configured minimum.
    void log(LogLevel level, const std::string& line);

    /// Lets a call site skip building an expensive line that wouldn't be written.
    [[nodiscard]] bool enabled(LogLevel level) const { return level >= minimum_; }

    /// Pushes anything buffered to disk; also called on any Info+ line and on destruction.
    void flush();

    /// Parses "debug"/"info"/"warning"/"error" (any case), or nullopt.
    [[nodiscard]] static std::optional<LogLevel> level_from_name(std::string_view name);

private:
    // Renames log_path_ to ".1" and reopens fresh. Called with mutex_ held.
    void rotate();

    LogLevel minimum_;
    std::filesystem::path log_path_;
    std::uintmax_t max_bytes_;
    std::uintmax_t bytes_written_ = 0;
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace kfc::protocol
