#pragma once

#include "protocol/frame_codec.hpp"
#include <chrono>
#include <memory>
#include <string>

// ========== 控制算法统一接口 ==========
class IController {
public:
    virtual ~IController() = default;

    // 输入传感器数据，输出决策结果（含控制命令 + 决策元数据）
    virtual DecisionResult decide(const SensorData& sensor) = 0;
};

// ========== 专家规则配置 ==========
struct ExpertRuleConfig {
    // 土壤目标
    float soilTargetMidline = 45.0f;   // 目标中线
    float soilLowerBound    = 35.0f;   // 允许下限
    float soilUpperBound    = 55.0f;   // 允许上限
    float soilEmergency     = 15.0f;   // 救命底线（待标定）

    // VPD 阈值 (kPa)
    float vpdUnlockThreshold   = 0.8f;  // VPD 主动驱动解锁阈值
    float vpdStressThreshold   = 2.0f;  // 干旱胁迫阈值
    float vpdHumidityLock      = 0.3f;  // 低于此值 = 空气窒息锁

    // 正午高温锁
    float noonTempThreshold      = 30.0f;  // 正午气温超过此值 → 锁死
    float noonSoilTempThreshold  = 28.0f;  // 正午地温超过此值 → 锁死（炸根直接原因）
    // 防重入
    int   cooldownSeconds     = 600;    // 浇水后最少间隔秒数（默认10分钟）

    // 时段定义（小时边界，可配置以适应实际日出日落）
    int   dawnStart       = 6;    // 清晨
    int   forenoonStart   = 8;    // 上午
    int   noonStart       = 12;   // 正午
    int   afternoonStart  = 14;   // 下午
    int   eveningStart    = 17;   // 傍晚
    int   nightStart      = 19;   // 夜间
};

// ========== 专家规则控制器 ==========
class ExpertRuleController : public IController {
public:
    explicit ExpertRuleController(const ExpertRuleConfig& cfg);
    DecisionResult decide(const SensorData& sensor) override;

private:
    ExpertRuleConfig cfg_;
    std::chrono::steady_clock::time_point lastWaterTime_ = std::chrono::steady_clock::now();
};

// ========== ML 推理控制器（v2，当前为桩） ==========
class MlController : public IController {
public:
    explicit MlController(const std::string& modelPath = "");
    DecisionResult decide(const SensorData& sensor) override;

private:
    std::string modelPath_;
    bool warned_ = false;
};
