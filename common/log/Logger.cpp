#include "common/log/Logger.h"

#include "common/utils/TimeUtil.h"

#include <iostream>
#include <sstream>
#include <thread>

namespace nebula {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger()
    : level_(LogLevel::INFO),
      console_enabled_(true) {}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::enableConsole(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_enabled_ = enabled;
}

bool Logger::setLogFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    file_stream_.open(file_path, std::ios::out | std::ios::app);
    if (!file_stream_.is_open()) {
        console_enabled_ = true;
        std::cerr << TimeUtil::nowString() << " [ERROR] [tid=" << std::this_thread::get_id()
                  << "] Logger.cpp:47 failed to open log file: " << file_path << std::endl;
        return false;
    }

    return true;
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }

    std::ostringstream oss;
    oss << TimeUtil::nowString()
        << " [" << levelToString(level) << "]"
        << " [tid=" << std::this_thread::get_id() << "] "
        << baseName(file) << ':' << line << ' '
        << message;

    const std::string line_text = oss.str();
    if (console_enabled_) {
        std::cerr << line_text << std::endl;
    }
    if (file_stream_.is_open()) {
        file_stream_ << line_text << std::endl;
    }
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
    }
    return "UNKNOWN";
}

const char* Logger::baseName(const char* path) {
    if (path == nullptr) {
        return "unknown";
    }

    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

}  // namespace nebula
