#include "core/pipeline.hpp"

extern "C" {
#include "uart.h"
}

#include "utils/logger.hpp"
#include <cstring>
#include <chrono>

// ========== Pipeline 构造 / 生命周期 ==========
Pipeline::Pipeline(int uartFd, CsvWriter* csv, std::unique_ptr<IController> controller,
                   int sensorBufMs)
    : uartFd_(uartFd)
    , csv_(csv)
    , controller_(std::move(controller))
    , sensorBufMs_(sensorBufMs)
{}

void Pipeline::start() {
    if (running_) return;
    running_ = true;
    threads_.emplace_back(&Pipeline::rxLoop, this);
    threads_.emplace_back(&Pipeline::processLoop, this);
    threads_.emplace_back(&Pipeline::txLoop, this);
    threads_.emplace_back(&Pipeline::csvLoop, this);
}

void Pipeline::stop() {
    if (!running_) return;
    running_ = false;
    rxQueue_.stop();
    csvQueue_.stop();
    txQueue_.stop();
}

void Pipeline::join() {
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

// ========== RX 线程：串口接收 + 帧同步 ==========
void Pipeline::rxLoop() {
    LOG_INFO("RX 线程启动");
    uint8_t buf[256];
    size_t  pos = 0;

    while (running_) {
        size_t n = 0;
        int ret = uart_read(uartFd_, buf + pos, 1, &n);
        if (ret != 0 || n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        ++pos;

        for (size_t i = 0; i + SENSOR_FRAME_SIZE <= pos; ) {
            if (buf[i] == UART_HEADER_1 && buf[i + 1] == UART_HEADER_2) {

                if (i + SENSOR_FRAME_SIZE > pos) break;

                const uint8_t* cand = &buf[i];
                if (cand[23] == UART_TAIL_1 && cand[24] == UART_TAIL_2
                    && FrameCodec::checksum(&cand[2], SENSOR_DATA_SIZE) == cand[22]) {

                    std::vector<uint8_t> frame(cand, cand + SENSOR_FRAME_SIZE);
                    rxQueue_.push(std::move(frame));

                    size_t consumed = i + SENSOR_FRAME_SIZE;
                    std::memmove(buf, buf + consumed, pos - consumed);
                    pos -= consumed;
                    i = 0;
                    continue;
                }
            }
            ++i;
        }

        if (pos > sizeof(buf) - SENSOR_FRAME_SIZE) {
            size_t keep = SENSOR_FRAME_SIZE;
            std::memmove(buf, buf + pos - keep, keep);
            pos = keep;
        }
    }
    LOG_INFO("RX 线程退出");
}

// ========== 处理线程：解析 → 缓冲平均 → 特征工程 → 控制决策 → 分派 ==========
void Pipeline::processLoop() {
    LOG_INFO("处理线程启动");
    sensorBufStart_ = std::chrono::steady_clock::now();

    while (running_) {
        std::vector<uint8_t> frame;
        if (!rxQueue_.pop(frame)) break; // 队列关闭且空了

        auto sensor = FrameCodec::parseSensorFrame(frame.data(), frame.size());
        if (!sensor) {
            LOG_WARN("处理: 帧解析失败，跳过");
            continue;
        }

        LOG_DEBUG("处理: 解析到传感器数据 (T=%.2f°C, RH=%.2f%%, SoilT=%.2f°C, SoilH=%.2f%%, Lux=%.1f)",
                  sensor->temperature, sensor->humidity,
                  sensor->soilTemperature, sensor->soilHumidity,
                  sensor->illuminance);

        // 累加传感器数据
        sensorSum_.temperature     += sensor->temperature;
        sensorSum_.humidity        += sensor->humidity;
        sensorSum_.soilTemperature += sensor->soilTemperature;
        sensorSum_.soilHumidity    += sensor->soilHumidity;
        sensorSum_.illuminance     += sensor->illuminance;
        ++sensorCount_;

        // 检查是否到达缓冲窗口
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - sensorBufStart_).count());
        if (elapsedMs < sensorBufMs_)
            continue;

        // 计算均值
        SensorData avgSensor;
        avgSensor.temperature     = sensorSum_.temperature     / sensorCount_;
        avgSensor.humidity        = sensorSum_.humidity        / sensorCount_;
        avgSensor.soilTemperature = sensorSum_.soilTemperature / sensorCount_;
        avgSensor.soilHumidity    = sensorSum_.soilHumidity    / sensorCount_;
        avgSensor.illuminance     = sensorSum_.illuminance     / sensorCount_;

        // 重置累加器
        sensorSum_ = SensorData{};
        sensorCount_ = 0;
        sensorBufStart_ = now;

        // 打时间戳 (Linux 系统时间)
        auto sysNow = std::chrono::system_clock::now().time_since_epoch();
        int64_t tsMs = std::chrono::duration_cast<std::chrono::milliseconds>(sysNow).count();

        // 控制决策（特征工程在控制器内部完成）
        DecisionResult decision = controller_->decide(avgSensor);

        LOG_DEBUG("处理: 决策结果 (cmd=%d, ruleTriggered=%d, actionType=%d)",
                  decision.cmd,
                  decision.ruleTriggered,
                  decision.actionType);

        // 组装扩展后的 CSV 行
        CsvRow row;
        row.timestampMs     = tsMs;
        row.sensor          = avgSensor;
        row.control         = decision.cmd;
        // AI 训练字段
        row.calculatedVpd   = decision.features.vpd_kPa;
        row.timeWindow      = decision.features.timeWindow;
        row.lightFlag       = decision.features.lightFlag;
        row.soilBuffer      = decision.features.soilBuffer;
        row.ruleTriggered   = decision.ruleTriggered;
        row.actionType      = decision.actionType;
        row.dataReliability = decision.dataReliability;

        csvQueue_.push(row);

        // 将控制命令序列化为帧，推入发送队列
        for (auto& [cId, payload] : decision.cmd.toFrames()) {
            txQueue_.push(FrameCodec::buildCommandFrame(cId, payload));
        }
    }
    LOG_INFO("处理线程退出");
}

// ========== TX 线程：串口发送 ==========
void Pipeline::txLoop() {
    LOG_INFO("TX 线程启动");

    while (running_) {
        std::vector<uint8_t> frame;
        if (!txQueue_.pop(frame)) break;

        size_t written = 0;
        int ret = uart_write(uartFd_, frame.data(), frame.size(), &written);
        if (ret != 0 || written != frame.size()) {
            LOG_WARN("TX: 发送失败 (ret=%d, written=%zu/%zu)", ret, written, frame.size());
        }
    }
    LOG_INFO("TX 线程退出");
}

// ========== CSV 线程：缓冲写盘 ==========
void Pipeline::csvLoop() {
    LOG_INFO("CSV 线程启动");

    while (running_) {
        CsvRow row;
        if (!csvQueue_.pop(row)) break;
        csv_->writeRow(row);
    }

    csv_->flush();
    LOG_INFO("CSV 线程退出");
}
