#pragma once
/*
 * ByBridge Daemon — Logger
 * Structured logging with severity levels and timestamps.
 */

#include <string>
#include <iostream>

namespace bybridge {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static void init(LogLevel minLevel = LogLevel::INFO);
    static void log(LogLevel level, const std::string& tag, const std::string& message);

    static void debug(const std::string& tag, const std::string& msg);
    static void info(const std::string& tag, const std::string& msg);
    static void warn(const std::string& tag, const std::string& msg);
    static void error(const std::string& tag, const std::string& msg);

private:
    static LogLevel s_minLevel;
    static const char* levelToString(LogLevel level);
    static std::string timestamp();
};

} // namespace bybridge
