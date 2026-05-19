#include "core/feature_engine.hpp"
#include "control/controller.hpp"  // ExpertRuleConfig
#include <cmath>

// ========== VPD 计算（Murray 1967 公式） ==========
float FeatureEngine::calcVPD(float temperature, float humidity) {
    float es = 0.61078f * std::exp(17.27f * temperature / (temperature + 237.3f));
    float vpd = es * (1.0f - humidity / 100.0f);
    return vpd > 0.0f ? vpd : 0.0f;
}

// ========== 时段分类 ==========
int FeatureEngine::classifyTimeWindow(int hours, int minutes,
                                       int dawnStart, int forenoonStart,
                                       int noonStart, int afternoonStart,
                                       int eveningStart, int nightStart) {
    int t = hours * 60 + minutes;

    int nightStartMin = nightStart * 60;
    int dawnStartMin  = dawnStart * 60;

    if (t >= nightStartMin || t < dawnStartMin)
        return TimeWindowCode::Night;

    if (t < forenoonStart * 60)
        return TimeWindowCode::Dawn;

    if (t < noonStart * 60)
        return TimeWindowCode::Forenoon;

    if (t < afternoonStart * 60)
        return TimeWindowCode::Noon;

    if (t < eveningStart * 60)
        return TimeWindowCode::Afternoon;

    return TimeWindowCode::Evening;
}

// ========== 光合活性推断 ==========
bool FeatureEngine::inferLightFlag(int timeWindow, float illuminance,
                                    float lightActiveMin, float lightDarkMax) {
    if (timeWindow == TimeWindowCode::Night)
        return false;

    if (illuminance > lightActiveMin)
        return true;

    if (illuminance < lightDarkMax)
        return false;

    return true;
}

// ========== 土壤缓冲差 ==========
float FeatureEngine::calcSoilBuffer(float soilHumidity, float targetMidline) {
    return soilHumidity - targetMidline;
}

// ========== 一键计算（hours/minutes 来自 Linux 系统时间） ==========
FeatureSnapshot FeatureEngine::computeAll(const SensorData& sensor,
                                           const ExpertRuleConfig& cfg,
                                           int hours, int minutes) {
    FeatureSnapshot snap;
    snap.vpd_kPa    = calcVPD(sensor.temperature, sensor.humidity);
    snap.timeWindow = classifyTimeWindow(
        hours, minutes,
        cfg.dawnStart, cfg.forenoonStart,
        cfg.noonStart, cfg.afternoonStart,
        cfg.eveningStart, cfg.nightStart);
    snap.lightFlag  = inferLightFlag(snap.timeWindow, sensor.illuminance,
                                     cfg.lightActiveMin, cfg.lightDarkMax);
    snap.soilBuffer = calcSoilBuffer(sensor.soilHumidity, cfg.soilTargetMidline);
    return snap;
}
