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
// accepts.
constexpr kfc::util::EnumNames<LogLevel, 4> kLevelNames{{{
    {LogLevel::Debug, "debug"},
    {LogLevel::Info, "info"},
    {LogLevel::Warning, "warning"},
    {LogLevel::Error, "error"},
}}};

static_assert(kLevelNames.covers_through(LogLevel::Error), "every LogLevel needs a name");

// "2026-07-28 14:03:11.482", written into a caller-owned buffer. snprintf
// instead of ostringstream/put_time since this runs once per logged line.
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
    // Start the byte count from any pre-existing content so rotation still
    // triggers at max_bytes_ total across restarts.
    std::error_code ec;
    std::uintmax_t existing_size = std::filesystem::file_size(log_path, ec);
    bytes_written_ = ec ? 0 : existing_size;
}

FileLogger::~FileLogger() {
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

    // Formatted before the lock is taken; built into one string so its size
    // is known for bytes_written_ without a tellp() call.
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
    std::filesystem::remove(rotated, ec);             // fine if there wasn't one yet
    std::filesystem::rename(log_path_, rotated, ec);  // best-effort: logging must never crash the server

    file_.open(log_path_, std::ios::app);
    bytes_written_ = 0;
}

}  // namespace kfc::protocol
