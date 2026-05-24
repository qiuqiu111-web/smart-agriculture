/* SoilHumidity.h - Soil humidity sensor (LM393) driver public types and API */
#ifndef SOIL_HUMIDITY_H
#define SOIL_HUMIDITY_H

#include "adc.h"
#include <stdint.h>
#include "func_config.h"

#define SOIL_HUMIDITY_MAX_RETRY_COUNT  3U
#define SOIL_HUMIDITY_SAMPLE_COUNT     15U   // 多次采样取平均的样本数
#define SOIL_HUMIDITY_WET           0.85f
#define SOIL_HUMIDITY_DRY           2.7f


typedef enum {
    SOIL_HUMIDITY_CALIBRATE=0, // 启动ADC转换并采样
    SOIL_HUMIDITY_READ,        // 读取ADC数据并计算湿度（保留）
    SOIL_HUMIDITY_COMPLETE,    // 数据就绪
} eSoilHumidity_status;

typedef enum {
    SOIL_HUMIDITY_OK = 0,
    SOIL_HUMIDITY_ERROR_ADC,      // ADC通信错误
    SOIL_HUMIDITY_ERROR_TIMEOUT,  // 超时错误
    SOIL_HUMIDITY_ERROR_DATA,     // 数据错误（超出范围）
} eSoilHumidity_error_code;

typedef struct {
    ADC_HandleTypeDef *hadc;       // ADC句柄指针
    eSoilHumidity_status status;   // 当前状态
    uint16_t raw_data;             // 原始ADC值
    float calib_dry;               // 干燥校准电压（传感器在空气中）
    float calib_wet;               // 湿润校准电压（传感器在水中）
    uint8_t calibrated;            // 校准完成标志（0:未校准, 1:已校准）
    uint8_t data_flag;             // 数据准备好标志（0:无数据, 1:数据就绪）
    uint8_t retry_count;           // 重试次数
    uint32_t start_time;           // 时间戳（ms）
    float humidity;                // 土壤湿度百分比（0.0~100.0）
} SoilHumidity_Handle;

// 初始化，返回 0 成功
int soil_humidity_init(SoilHumidity_Handle *handle);

// 状态机驱动，非阻塞，需定期调用
void soil_humidity_run(SoilHumidity_Handle *handle);

// 获取已准备好的数据（若无数据返回非 0）
int soil_humidity_get(SoilHumidity_Handle *handle, float *humidity);

// 数据就绪检查，返回 0 表示数据准备好
int soil_humidity_ready(SoilHumidity_Handle *handle);

// 设置校准电压值（需用户自行标定）
// dry - 传感器在干燥空气中的电压（V）
// wet - 传感器完全浸入水中的电压（V）
int soil_humidity_set_calib(SoilHumidity_Handle *handle, float dry, float wet);

#endif // SOIL_HUMIDITY_H
