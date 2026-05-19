#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

// ========== 帧定界符（与 STM32 端一致） ==========
constexpr uint8_t UART_HEADER_1 = 0xAA;
constexpr uint8_t UART_HEADER_2 = 0x55;
constexpr uint8_t UART_TAIL_1   = 0x55;
constexpr uint8_t UART_TAIL_2   = 0xAA;

// ========== 紧凑线格式（与 STM32 的 Sensors_Data 一一对应，5×float=20 字节） ==========
#pragma pack(push, 1)
struct SensorDataWire {
    float temperature;       // 空气温度（AHT30）
    float humidity;          // 空气湿度（AHT30）
    float soilTemperature;   // 土壤温度（DS18B20）
    float soilHumidity;      // 土壤湿度
    float illuminance;       // 光照强度
};
#pragma pack(pop)
static_assert(sizeof(SensorDataWire) == 20, "SensorDataWire must be 20 bytes");

constexpr size_t SENSOR_DATA_SIZE  = sizeof(SensorDataWire);   // 20
constexpr size_t SENSOR_FRAME_SIZE = SENSOR_DATA_SIZE + 5;     // 25 = 帧头2 + 数据20 + 校验1 + 帧尾2
constexpr size_t CMD_DATA_SIZE     = 2;
constexpr size_t CMD_FRAME_SIZE    = CMD_DATA_SIZE + 5;        // 7

// ========== 逻辑层传感器数据（解包后，时间由 Linux 端补充） ==========
struct SensorData {
    float temperature     = 0.0f;   // 空气温度 (°C)
    float humidity        = 0.0f;   // 空气湿度 (%)
    float soilTemperature = 0.0f;   // 土壤温度 (°C)
    float soilHumidity    = 0.0f;   // 土壤湿度
    float illuminance     = 0.0f;   // 光照强度 (lux)

    static SensorData fromWire(const SensorDataWire& w);
};

// ========== 控制命令（与 STM32 端一致，payload=1 触发 0.5s 脉冲） ==========
struct ControlCmd {
    bool pump   = false;   // 水泵（1=脉冲一次）

    // 展开为 (命令字, 载荷) 序列
    std::vector<std::pair<uint8_t, uint8_t>> toFrames() const;
};

// 命令字定义（与 STM32 端一致）
constexpr uint8_t CMD_PUMP   = 0x01;

// ========== 特征工程快照（由传感器原始值 + Linux 时间计算得到） ==========
struct FeatureSnapshot {
    float vpd_kPa     = 0.0f;   // 饱和水汽压差 (kPa)
    int   timeWindow  = 0;      // 时段枚举，见 TimeWindow 常量
    bool  lightFlag   = false;  // 光合活性推断
    float soilBuffer  = 0.0f;   // 当前湿度 - 目标中线
};

// ========== 时段枚举 ==========
namespace TimeWindowCode {
    constexpr int Dawn      = 0;  // 清晨启动期 06:00-08:00
    constexpr int Forenoon  = 1;  // 上午光合期 08:00-12:00
    constexpr int Noon      = 2;  // 正午保护期 12:00-14:00
    constexpr int Afternoon = 3;  // 下午光合期 14:00-17:00
    constexpr int Evening   = 4;  // 傍晚回补期 17:00-19:00
    constexpr int Night     = 5;  // 夜间休眠期 19:00-06:00
}

// ========== 规则触发枚举 ==========
namespace RuleTriggeredCode {
    constexpr int None              = 0;
    constexpr int Night_Lock        = 1;   // L1 夜间休眠锁
    constexpr int Noon_Lock         = 2;   // L2 正午高温锁
    constexpr int Humidity_Lock     = 3;   // L3 空气窒息锁
    constexpr int Reentry_Lock      = 4;   // L4 防重入锁
    constexpr int Emergency_Save    = 5;   // L5 救命底线锁
    constexpr int Morning_Preventive= 6;   // 清晨预防补水
    constexpr int VPD_Drive_Morning = 7;   // 上午 VPD 主动驱动
    constexpr int VPD_Drive_Afternoon=8;   // 下午 VPD 驱动（降级）
    constexpr int Evening_Replenish = 9;   // 傍晚回补
}

// ========== 动作类型枚举 ==========
namespace ActionTypeCode {
    constexpr int Null             = 0;
    constexpr int VPD_Active       = 1;   // VPD 环境驱动
    constexpr int Preventive       = 2;   // 预防性补水
    constexpr int Emergency_Save   = 3;   // 救命补水
}

// ========== 决策结果（替代裸 ControlCmd） ==========
struct DecisionResult {
    ControlCmd cmd;                   // 串口命令（保持原格式）
    int  ruleTriggered = RuleTriggeredCode::None;
    int  actionType    = ActionTypeCode::Null;
    bool dataReliability = true;      // 本次土壤湿度是否可信

    // 附带特征快照，供 CSV 写入
    FeatureSnapshot features;
};

// ========== CSV 行数据（扩展 AI 训练字段） ==========
struct CsvRow {
    int64_t    timestampMs;  // Linux 端接收时间戳（毫秒）
    SensorData sensor;       // 传感器输入
    ControlCmd control;      // 控制输出

    // 以下为 AI 训练扩展字段
    float calculatedVpd  = 0.0f;   // 计算得到的 VPD
    int   timeWindow     = 0;      // 时段分类
    bool  lightFlag      = false;  // 光合活性推断
    float soilBuffer     = 0.0f;   // 土壤缓冲差
    int   ruleTriggered  = 0;      // 命中规则
    int   actionType     = 0;      // 动作类型
    bool  dataReliability= true;   // 数据可靠性标记
};

// ========== 纯协议函数（与硬件无关，可单元测试） ==========
class FrameCodec {
public:
    // 逐字节异或校验和
    static uint8_t checksum(const uint8_t* data, size_t len);

    // 解析 25 字节传感器帧，失败返回 std::nullopt
    static std::optional<SensorData> parseSensorFrame(const uint8_t* buf, size_t len);

    // 用 cmdId + payload 构建 7 字节命令帧
    static std::vector<uint8_t> buildCommandFrame(uint8_t cmdId, uint8_t payload);
};
