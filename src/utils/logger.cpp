#include "utils/logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <mutex>

namespace bybridge {

LogLevel Logger::s_minLevel = LogLevel::INFO;
static std::mutex s_logMutex;

void Logger::init(LogLevel minLevel) {
    s_minLevel = minLevel;
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_r(&time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& tag, const std::string& message) {
    if (level < s_minLevel) return;

    std::lock_guard<std::mutex> lock(s_logMutex);
    std::cerr << "[" << timestamp() << "] "
              << "[" << levelToString(level) << "] "
              << "[" << tag << "] "
              << message << std::endl;
}

void Logger::debug(const std::string& tag, const std::string& msg) { log(LogLevel::DEBUG, tag, msg); }
void Logger::info(const std::string& tag, const std::string& msg)  { log(LogLevel::INFO, tag, msg); }
void Logger::warn(const std::string& tag, const std::string& msg)  { log(LogLevel::WARN, tag, msg); }
void Logger::error(const std::string& tag, const std::string& msg) { log(LogLevel::ERROR, tag, msg); }

} // namespace bybridge
