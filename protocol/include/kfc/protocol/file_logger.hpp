#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace kfc::protocol {

/// A timestamped line-appender, used by both kfc_server and kfc_gui_app's
/// networked ServerLink to log every protocol message sent/received -- the
/// primary debugging tool the CTD SERVER lecture explicitly asked for
/// ("שנוכל מתוך הלוגים להבין מה התקלקל"). Deliberately minimal: no log
/// levels, no rotation, no external logging library -- a single append-only
/// file is exactly what "read it after something breaks" needs at this
/// scale. Thread-safe (one mutex): the server's per-match tick thread and
/// its IXWebSocket I/O threads can all log without external synchronization.
class FileLogger {
public:
    /// Opens (or creates) log_path in append mode. Throws std::runtime_error
    /// if the file cannot be opened -- a server/client that cannot log
    /// should fail loudly at startup, not silently run undebuggable.
    explicit FileLogger(const std::filesystem::path& log_path);

    /// Appends one line, prefixed with a wall-clock timestamp, and flushes
    /// immediately -- so the last line is never lost to a crash that never
    /// got to close the file.
    void log(const std::string& line);

private:
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace kfc::protocol
