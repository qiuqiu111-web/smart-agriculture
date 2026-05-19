#pragma once

#include "protocol/frame_codec.hpp"
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

class CsvWriter {
public:
    // dataDir:   输出目录，如 "data"
    // flushMs:   每隔多少毫秒强制刷新一次
    // bufferRows:缓冲区最大行数，达到后自动刷新
    CsvWriter(const std::string& dataDir, int flushMs, int bufferRows);
    ~CsvWriter();

    void writeRow(const CsvRow& row);
    void flush();
    void close();

private:
    void checkRotate();                    // 跨天时自动切分文件
    void ensureHeader();                   // 新建文件时写入表头
    static std::string currentDate();      // 返回当前日期 "YYYY-MM-DD"

    std::string dataDir_;
    int flushMs_;
    int bufferRows_;

    std::ofstream ofs_;
    std::vector<CsvRow> buffer_;
    std::chrono::steady_clock::time_point lastFlush_;
    std::string currentFile_;
    std::mutex mutex_;
};
