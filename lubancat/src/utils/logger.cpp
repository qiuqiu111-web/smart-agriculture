#include "utils/logger.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

LogLevel logLevelFromString(const std::string& s) {
    if (s == "debug") return LogLevel::DEBUG;
    if (s == "info")  return LogLevel::INFO;
    if (s == "warn")  return LogLevel::WARN;
    if (s == "error") return LogLevel::ERROR;
    return LogLevel::INFO;
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    shutdown();
}

void Logger::init(LogLevel level, const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
    if (!file.empty()) {
        file_.open(file, std::ios::app);
        useFile_ = file_.is_open();
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
        useFile_ = false;
    }
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    if (level < level_) return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    write(level, buf);
}

void Logger::debug(const char* fmt, ...) {
    if (LogLevel::DEBUG < level_) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(LogLevel::DEBUG, buf);
}

void Logger::info(const char* fmt, ...) {
    if (LogLevel::INFO < level_) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(LogLevel::INFO, buf);
}

void Logger::warn(const char* fmt, ...) {
    if (LogLevel::WARN < level_) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(LogLevel::WARN, buf);
}

void Logger::error(const char* fmt, ...) {
    if (LogLevel::ERROR < level_) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(LogLevel::ERROR, buf);
}

void Logger::write(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    auto tm  = *std::localtime(&tt);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();

    const char* tag = "?";
    switch (level) {
        case LogLevel::DEBUG: tag = "DEBUG"; break;
        case LogLevel::INFO:  tag = "INFO "; break;
        case LogLevel::WARN:  tag = "WARN "; break;
        case LogLevel::ERROR: tag = "ERROR"; break;
    }
    oss << " [" << tag << "] " << msg;

    std::string line = oss.str();
    if (useFile_ && file_.is_open()) {
        file_ << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }
}
