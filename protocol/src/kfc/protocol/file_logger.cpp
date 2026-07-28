#include "kfc/protocol/file_logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace kfc::protocol {

namespace {

std::string timestamp_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream out;
    out << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return out.str();
}

}  // namespace

FileLogger::FileLogger(const std::filesystem::path& log_path) : file_(log_path, std::ios::app) {
    if (!file_) {
        throw std::runtime_error("FileLogger: cannot open log file: " + log_path.string());
    }
}

void FileLogger::log(const std::string& line) {
    std::lock_guard<std::mutex> guard(mutex_);
    file_ << '[' << timestamp_now() << "] " << line << '\n';
    file_.flush();
}

}  // namespace kfc::protocol
