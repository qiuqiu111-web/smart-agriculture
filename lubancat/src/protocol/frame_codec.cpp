#include "protocol/frame_codec.hpp"
#include <cstring>

// ========== SensorData：将紧凑线格式转换为逻辑结构 ==========
SensorData SensorData::fromWire(const SensorDataWire& w) {
    SensorData d;
    d.temperature     = w.temperature;
    d.humidity        = w.humidity;
    d.soilTemperature = w.soilTemperature;
    d.soilHumidity    = w.soilHumidity;
    d.illuminance     = w.illuminance;
    return d;
}

// ========== ControlCmd：展开为命令帧（当前仅水泵） ==========
std::vector<std::pair<uint8_t, uint8_t>> ControlCmd::toFrames() const {
    std::vector<std::pair<uint8_t, uint8_t>> frames;
    frames.emplace_back(CMD_PUMP, pump ? 1 : 0);
    return frames;
}

// ========== FrameCodec：纯协议函数，与硬件无关 ==========

// 逐字节异或校验和（与 STM32 端算法一致）
uint8_t FrameCodec::checksum(const uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; ++i) {
        cs ^= data[i];
    }
    return cs;
}

// 解析 25 字节传感器帧: 帧头2 + 数据20 + 校验1 + 帧尾2
std::optional<SensorData> FrameCodec::parseSensorFrame(const uint8_t* buf, size_t len) {
    if (len < SENSOR_FRAME_SIZE) return std::nullopt;

    // 在缓冲区中扫描帧头 0xAA 0x55
    size_t start = 0;
    for (; start + SENSOR_FRAME_SIZE <= len; ++start) {
        if (buf[start] == UART_HEADER_1 && buf[start + 1] == UART_HEADER_2) break;
    }
    if (start + SENSOR_FRAME_SIZE > len) return std::nullopt;

    const uint8_t* frame = buf + start;

    // 校验帧尾 0x55 0xAA（字节 23-24）
    if (frame[23] != UART_TAIL_1 || frame[24] != UART_TAIL_2) return std::nullopt;

    // 校验和：对字节 2~21（20 字节数据区）做异或，与第 22 字节比对
    if (checksum(&frame[2], SENSOR_DATA_SIZE) != frame[22]) return std::nullopt;

    // 反序列化
    SensorDataWire wire;
    std::memcpy(&wire, &frame[2], SENSOR_DATA_SIZE);
    return SensorData::fromWire(wire);
}

// 构建 7 字节命令帧：帧头 2 + 命令字 1 + 载荷 1 + 校验 1 + 帧尾 2
std::vector<uint8_t> FrameCodec::buildCommandFrame(uint8_t cmdId, uint8_t payload) {
    std::vector<uint8_t> frame(CMD_FRAME_SIZE);
    frame[0] = UART_HEADER_1;
    frame[1] = UART_HEADER_2;
    frame[2] = cmdId;
    frame[3] = payload;
    frame[4] = checksum(&frame[2], CMD_DATA_SIZE);
    frame[5] = UART_TAIL_1;
    frame[6] = UART_TAIL_2;
    return frame;
}
