#pragma once

#include "protocol/frame_codec.hpp"

struct ExpertRuleConfig;

// 纯函数集合，从传感器原始数据 + Linux 系统时间计算高维特征
class FeatureEngine {
public:
    // VPD (kPa) —— Murray (1967) 公式
    // T: 空气温度 (°C),  RH: 相对湿度 (%)
    static float calcVPD(float temperature, float humidity);

    // 时段分类（时间来自 Linux 端 system_clock）
    // 返回 TimeWindowCode 枚举值
    static int classifyTimeWindow(int hours, int minutes,
                                  int dawnStart, int forenoonStart,
                                  int noonStart, int afternoonStart,
                                  int eveningStart, int nightStart);
    // 土壤缓冲差 = 当前湿度 - 目标中线
    static float calcSoilBuffer(float soilHumidity, float targetMidline);

    // 一键计算全部特征（hours/minutes 来自 Linux 系统时间）
    static FeatureSnapshot computeAll(const SensorData& sensor,
                                      const ExpertRuleConfig& cfg,
                                      int hours, int minutes);
};
