#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3
};

LogLevel logLevelFromString(const std::string& s);

// 线程安全的单例日志器
class Logger {
public:
    // 单例
    static Logger& instance();

    void init(LogLevel level, const std::string& file = "");
    void shutdown();

    void log(LogLevel level, const char* fmt, ...);

    void debug(const char* fmt, ...);
    void info(const char* fmt, ...);
    void warn(const char* fmt, ...);
    void error(const char* fmt, ...);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write(LogLevel level, const std::string& msg);

    LogLevel level_ = LogLevel::INFO;
    std::ofstream file_;
    std::mutex mutex_;
    bool useFile_ = false;
};

#define LOG_DEBUG(fmt, ...) Logger::instance().debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::instance().info(fmt,  ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::instance().warn(fmt,  ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::instance().error(fmt, ##__VA_ARGS__)
