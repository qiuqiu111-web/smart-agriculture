#include "datalog/csv_writer.hpp"
#include "utils/logger.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

CsvWriter::CsvWriter(const std::string& dataDir, int flushMs, int bufferRows)
    : dataDir_(dataDir)
    , flushMs_(flushMs)
    , bufferRows_(bufferRows)
    , lastFlush_(std::chrono::steady_clock::now())
{
    checkRotate();
}

CsvWriter::~CsvWriter() {
    close();
}

void CsvWriter::writeRow(const CsvRow& row) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.push_back(row);

    if (buffer_.size() >= static_cast<size_t>(bufferRows_)) {
        flush();
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlush_).count() >= flushMs_) {
        flush();
    }
}

void CsvWriter::flush() {
    if (buffer_.empty()) return;

    checkRotate();
    ensureHeader();

    for (const auto& row : buffer_) {
        // 从 Linux 时间戳解析年月日时分秒
        std::time_t t = static_cast<std::time_t>(row.timestampMs / 1000);
        std::tm* local = std::localtime(&t);

        ofs_ // Linux 时间字段
             << (local->tm_year + 1900) << ','
             << (local->tm_mon  + 1)    << ','
             << local->tm_mday          << ','
             << local->tm_hour          << ','
             << local->tm_min           << ','
             << local->tm_sec           << ','
             // 传感器原始值
             << row.sensor.temperature     << ','
             << row.sensor.humidity        << ','
             << row.sensor.soilTemperature << ','
             << row.sensor.soilHumidity    << ','
             << row.sensor.illuminance     << ','
             // 控制输出
             << row.control.pump   << ','
             // 特征工程值
             << row.calculatedVpd  << ','
             << row.timeWindow     << ','
             << row.lightFlag      << ','
             << row.soilBuffer     << ','
             // 决策过程
             << row.ruleTriggered  << ','
             << row.actionType     << ','
             // 数据质量
             << row.dataReliability
             << '\n';
    }
    ofs_.flush();
    buffer_.clear();
    lastFlush_ = std::chrono::steady_clock::now();
}

void CsvWriter::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    flush();
    if (ofs_.is_open()) {
        ofs_.close();
    }
}

void CsvWriter::checkRotate() {
    std::string date = currentDate();
    std::string want = dataDir_ + "/" + date + ".csv";
    if (want != currentFile_) {
        if (ofs_.is_open()) ofs_.close();
        currentFile_ = want;
    }
}

void CsvWriter::ensureHeader() {
    if (!ofs_.is_open()) {
        ofs_.open(currentFile_, std::ios::app);
        if (!ofs_) {
            LOG_ERROR("CsvWriter: 无法打开 %s", currentFile_.c_str());
            return;
        }
        ofs_.seekp(0, std::ios::end);
        if (ofs_.tellp() == 0) {
            ofs_ << "year,month,date,hours,minutes,seconds,"
                    "temperature,humidity,soil_temperature,soil_humidity,illuminance,"
                    "pump,"
                    "calculated_vpd,time_window,light_flag,soil_buffer,"
                    "rule_triggered,action_type,"
                    "data_reliability\n";
            ofs_.flush();
        }
    }
}

std::string CsvWriter::currentDate() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}
