#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace kfc::protocol {

/// How much a line matters, which decides both whether it is written at all
/// and whether it is pushed to disk immediately.
///
/// Ordered, and compared as such -- a logger set to Info writes Info, Warning
/// and Error, and drops Debug.
enum class LogLevel {
    /// Every protocol message sent and received, in full. The CTD SERVER
    /// lecture asks for exactly this ("שנוכל מתוך הלוגים להבין מה התקלקל"), so
    /// it is on by default -- but it is also, by a wide margin, the most
    /// frequent thing written, which is why it is the one level that is
    /// buffered rather than flushed.
    Debug,
    /// Things that happened: someone joined, a room opened, a match ended.
    /// One per event, not one per frame.
    Info,
    /// Something is wrong but the server carries on.
    Warning,
    /// Something failed.
    Error,
};

/// A timestamped line-appender, used by both kfc_server and kfc_gui_app's
/// networked ServerLink to log protocol traffic and events -- the primary
/// debugging tool the CTD SERVER lecture explicitly asked for. Deliberately
/// minimal: no rotation, no external logging library -- a single append-only
/// file is exactly what "read it after something breaks" needs at this scale.
///
/// Thread-safe (one mutex): the server's per-match tick thread and its
/// IXWebSocket I/O threads can all log without external synchronization.
///
/// **What is flushed, and why not everything.** Every line used to be flushed
/// as it was written, which meant a write syscall per protocol message, on the
/// tick thread, with the mutex held. But the reason for flushing is real: a line
/// still sitting in a buffer when the process dies is a line you cannot read
/// afterwards, and the last line before a crash is usually the interesting one.
///
/// The resolution is that here, frequency and importance run opposite ways.
/// Debug is the message-by-message traffic -- many per second, individually
/// unremarkable -- so it is buffered. Everything above it is one line per actual
/// event, rare enough that flushing costs nothing, so it is flushed. A crash
/// therefore costs you the tail of the traffic dump, never the record of what
/// happened. The destructor flushes whatever is left.
class FileLogger {
public:
    /// Opens (or creates) log_path in append mode. Throws std::runtime_error
    /// if the file cannot be opened -- a server/client that cannot log
    /// should fail loudly at startup, not silently run undebuggable.
    ///
    /// minimum is the least severe level that will be written; the default
    /// keeps every message, which is what the spec asks of the server.
    explicit FileLogger(const std::filesystem::path& log_path, LogLevel minimum = LogLevel::Debug);

    /// Flushes anything still buffered.
    ~FileLogger();

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    /// Appends one line at Info, prefixed with a wall-clock timestamp.
    void log(const std::string& line);

    /// Appends one line at the given level. Dropped without being written if
    /// level is below the configured minimum.
    void log(LogLevel level, const std::string& line);

    /// Whether a line at this level would be written. For call sites where
    /// *building* the line costs something -- encoding a message, redacting a
    /// password out of one -- so that work can be skipped too, not just the
    /// write. A lock-free atomic read.
    [[nodiscard]] bool enabled(LogLevel level) const {
        return level >= minimum_.load(std::memory_order_relaxed);
    }

    /// Changes the least severe level written, at any time.
    void set_minimum(LogLevel minimum) { minimum_.store(minimum, std::memory_order_relaxed); }

    /// Pushes anything buffered to disk. Called for you on any line above
    /// Debug and on destruction; public for a caller that wants the traffic
    /// dump on disk at a particular moment.
    void flush();

    /// The level that name spells ("debug", "info", "warning", "error", in any
    /// case), or std::nullopt if it spells none of them -- so a command-line
    /// flag can reject a typo rather than silently pick a default.
    [[nodiscard]] static std::optional<LogLevel> level_from_name(std::string_view name);

private:
    std::atomic<LogLevel> minimum_;
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace kfc::protocol
