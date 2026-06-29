#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace nebula {

enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    void enableConsole(bool enabled);
    bool setLogFile(const std::string& file_path);

    void log(LogLevel level, const char* file, int line, const std::string& message);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static const char* levelToString(LogLevel level);
    static const char* baseName(const char* path);

private:
    std::mutex mutex_;
    LogLevel level_;
    bool console_enabled_;
    std::ofstream file_stream_;
};

}  // namespace nebula

#define LOG_DEBUG(message) ::nebula::Logger::instance().log(::nebula::LogLevel::DEBUG, __FILE__, __LINE__, (message))
#define LOG_INFO(message) ::nebula::Logger::instance().log(::nebula::LogLevel::INFO, __FILE__, __LINE__, (message))
#define LOG_WARN(message) ::nebula::Logger::instance().log(::nebula::LogLevel::WARN, __FILE__, __LINE__, (message))
#define LOG_ERROR(message) ::nebula::Logger::instance().log(::nebula::LogLevel::ERROR, __FILE__, __LINE__, (message))
#define LOG_FATAL(message) ::nebula::Logger::instance().log(::nebula::LogLevel::FATAL, __FILE__, __LINE__, (message))
