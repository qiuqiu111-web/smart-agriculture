#include "control/controller.hpp"
#include "core/feature_engine.hpp"
#include "utils/logger.hpp"
#include <ctime>

ExpertRuleController::ExpertRuleController(const ExpertRuleConfig& cfg)
    : cfg_(cfg) {}

DecisionResult ExpertRuleController::decide(const SensorData& sensor) {
    // ---- Step 0: 获取 Linux 系统时间，用于时段分类 ----
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* local = std::localtime(&t);

    FeatureSnapshot feat = FeatureEngine::computeAll(
        sensor, cfg_, local->tm_hour, local->tm_min);

    DecisionResult result;
    result.features         = feat;
    result.dataReliability  = true;

    // 距上次浇水的实际秒数
    auto steadyNow = std::chrono::steady_clock::now();
    int elapsedSec = static_cast<int>(
        std::chrono::duration_cast<std::chrono::seconds>(steadyNow - lastWaterTime_).count());

    // ---- Step 1: 救命底线锁 (L5) — 越权穿透 L1-L3 环境锁 ----
    if (sensor.soilHumidity < cfg_.soilEmergency) {
        if (elapsedSec < cfg_.cooldownSeconds / 2) {
            result.cmd.pump       = false;
            result.ruleTriggered  = RuleTriggeredCode::Reentry_Lock;
            result.actionType     = ActionTypeCode::Null;
            result.dataReliability = false;
            return result;
        }

        result.cmd.pump       = true;
        result.ruleTriggered  = RuleTriggeredCode::Emergency_Save;
        result.actionType     = ActionTypeCode::Emergency_Save;
        result.dataReliability = false;
        lastWaterTime_ = steadyNow;
        LOG_WARN("EMERGENCY: 土壤湿度 %.1f%% < 救命线 %.1f%%，强制执行脉冲",
                 sensor.soilHumidity, cfg_.soilEmergency);
        return result;
    }

    // ---- Step 2: 硬锁检查 (L1 → L3) ----
    if (feat.timeWindow == TimeWindowCode::Night) {
        result.cmd.pump       = false;
        result.ruleTriggered  = RuleTriggeredCode::Night_Lock;
        result.actionType     = ActionTypeCode::Null;
        return result;
    }

    if (feat.timeWindow == TimeWindowCode::Noon
        && (sensor.temperature > cfg_.noonTempThreshold
            || sensor.soilTemperature > cfg_.noonSoilTempThreshold)) {
        result.cmd.pump       = false;
        result.ruleTriggered  = RuleTriggeredCode::Noon_Lock;
        result.actionType     = ActionTypeCode::Null;
        return result;
    }

    if (feat.vpd_kPa < cfg_.vpdHumidityLock) {
        result.cmd.pump       = false;
        result.ruleTriggered  = RuleTriggeredCode::Humidity_Lock;
        result.actionType     = ActionTypeCode::Null;
        return result;
    }

    // ---- Step 3: 防重入锁 (L4) ----
    if (elapsedSec < cfg_.cooldownSeconds) {
        result.cmd.pump       = false;
        result.ruleTriggered  = RuleTriggeredCode::Reentry_Lock;
        result.actionType     = ActionTypeCode::Null;
        return result;
    }

    // ---- Step 4: 时段策略匹配 + VPD 驱动 ----
    bool shouldWater = false;
    int  rule        = RuleTriggeredCode::None;
    int  action      = ActionTypeCode::Null;

    switch (feat.timeWindow) {

    case TimeWindowCode::Dawn:
        if (sensor.soilHumidity < cfg_.soilLowerBound) {
            shouldWater = true;
            rule   = RuleTriggeredCode::Morning_Preventive;
            action = ActionTypeCode::Preventive;
        }
        break;

    case TimeWindowCode::Forenoon:
        if (feat.vpd_kPa > cfg_.vpdUnlockThreshold
            && sensor.soilHumidity < cfg_.soilUpperBound) {
            shouldWater = true;
            rule   = RuleTriggeredCode::VPD_Drive_Morning;
            action = ActionTypeCode::VPD_Active;
        } else if (feat.vpd_kPa > cfg_.vpdStressThreshold) {
            shouldWater = true;
            rule   = RuleTriggeredCode::VPD_Drive_Morning;
            action = ActionTypeCode::VPD_Active;
        }
        break;

    case TimeWindowCode::Noon:
        if (feat.vpd_kPa > cfg_.vpdStressThreshold
            && sensor.soilHumidity < cfg_.soilLowerBound) {
            shouldWater = true;
            rule   = RuleTriggeredCode::VPD_Drive_Morning;
            action = ActionTypeCode::VPD_Active;
        }
        break;

    case TimeWindowCode::Afternoon:
        if (feat.vpd_kPa > cfg_.vpdStressThreshold
            && sensor.soilHumidity < cfg_.soilLowerBound) {
            shouldWater = true;
            rule   = RuleTriggeredCode::VPD_Drive_Afternoon;
            action = ActionTypeCode::VPD_Active;
        }
        break;

    case TimeWindowCode::Evening:
        if (feat.soilBuffer < 0.0f) {
            shouldWater = true;
            rule   = RuleTriggeredCode::Evening_Replenish;
            action = ActionTypeCode::Preventive;
        }
        break;

    case TimeWindowCode::Night:
    default:
        break;
    }

    result.cmd.pump      = shouldWater;
    result.ruleTriggered = rule;
    result.actionType    = action;

    if (shouldWater) {
        result.dataReliability = false;
        lastWaterTime_ = steadyNow;
        LOG_INFO("浇水决策: 时段=%d VPD=%.2f soil=%.1f%% rule=%d",
                 feat.timeWindow, feat.vpd_kPa,
                 sensor.soilHumidity, rule);
    }

    return result;
}
