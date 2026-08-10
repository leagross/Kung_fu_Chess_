#include "kfc/protocol/file_logger.hpp"

#include "kfc/util/enum_names.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>
#include <system_error>

namespace kfc::protocol {

namespace {

// The tag written in front of each line, and the names the command-line flag
// accepts. One table, both directions -- see kfc/util/enum_names.hpp.
constexpr kfc::util::EnumNames<LogLevel, 4> kLevelNames{{{
    {LogLevel::Debug, "debug"},
    {LogLevel::Info, "info"},
    {LogLevel::Warning, "warning"},
    {LogLevel::Error, "error"},
}}};

static_assert(kLevelNames.covers_through(LogLevel::Error), "every LogLevel needs a name");

// "2026-07-28 14:03:11.482", written into a caller-owned buffer.
//
// Formatted with snprintf rather than an ostringstream and std::put_time: this
// runs once per logged line, and the stream version allocated a std::string and
// went through the locale machinery to produce twenty-three fixed-width
// characters. It is also called before the mutex is taken, so no thread waits
// on another one's clock formatting.
void write_timestamp(std::array<char, 32>& out) {
    auto now = std::chrono::system_clock::now();
    std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &seconds);
#else
    localtime_r(&seconds, &local_time);
#endif

    std::snprintf(out.data(), out.size(), "%04d-%02d-%02d %02d:%02d:%02d.%03d", local_time.tm_year + 1900,
                  local_time.tm_mon + 1, local_time.tm_mday, local_time.tm_hour, local_time.tm_min,
                  local_time.tm_sec, static_cast<int>(ms.count()));
}

}  // namespace

FileLogger::FileLogger(const std::filesystem::path& log_path, LogLevel minimum, std::uintmax_t max_bytes)
    : minimum_(minimum), log_path_(log_path), max_bytes_(max_bytes), file_(log_path, std::ios::app) {
    if (!file_) {
        throw std::runtime_error("FileLogger: cannot open log file: " + log_path.string());
    }
    // Appending to a file that already had content from a previous run --
    // the common case for a restarted server -- starts the byte count from
    // where that content left off, so rotation still triggers at max_bytes_
    // total rather than resetting the clock on every restart.
    std::error_code ec;
    std::uintmax_t existing_size = std::filesystem::file_size(log_path, ec);
    bytes_written_ = ec ? 0 : existing_size;
}

FileLogger::~FileLogger() {
    // Whatever Debug traffic is still buffered belongs on disk before the file
    // closes -- an orderly shutdown must not be the thing that loses it.
    flush();
}

std::optional<LogLevel> FileLogger::level_from_name(std::string_view name) {
    std::string lowered;
    lowered.reserve(name.size());
    for (char c : name) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return kLevelNames.value_of(lowered);
}

void FileLogger::log(const std::string& line) {
    log(LogLevel::Info, line);
}

void FileLogger::log(LogLevel level, const std::string& line) {
    if (!enabled(level)) {
        return;
    }

    // Both done before the lock: the timestamp because formatting it is the
    // most expensive part of writing a line, and the tag because looking it up
    // is a table walk. Neither touches the file. Built into one string rather
    // than streamed piece by piece so its size is known for bytes_written_
    // without a tellp() call.
    std::array<char, 32> timestamp{};
    write_timestamp(timestamp);
    std::string_view tag = kLevelNames.name_of(level);

    std::string formatted;
    formatted.reserve(timestamp.size() + tag.size() + line.size() + 8);
    formatted.push_back('[');
    formatted.append(timestamp.data());
    formatted.append("] [");
    formatted.append(tag);
    formatted.append("] ");
    formatted.append(line);
    formatted.push_back('\n');

    std::lock_guard<std::mutex> guard(mutex_);
    file_ << formatted;
    bytes_written_ += formatted.size();
    if (level > LogLevel::Debug) {
        // Rare by construction -- one line per event -- so paying for the write
        // here buys crash-visibility for everything that matters, without the
        // per-message traffic paying for it too. See the header.
        file_.flush();
    }
    if (bytes_written_ >= max_bytes_) {
        rotate();
    }
}

void FileLogger::flush() {
    std::lock_guard<std::mutex> guard(mutex_);
    file_.flush();
}

void FileLogger::rotate() {
    file_.close();

    std::error_code ec;
    std::filesystem::path rotated = log_path_;
    rotated += ".1";
    std::filesystem::remove(rotated, ec);      // fine if there wasn't one yet
    std::filesystem::rename(log_path_, rotated, ec);  // best-effort: logging
                                                       // must never be why the
                                                       // server crashes

    file_.open(log_path_, std::ios::app);
    bytes_written_ = 0;
    // Not reported through log() -- the file that would receive it is the one
    // that just changed underneath this call.
}

}  // namespace kfc::protocol
