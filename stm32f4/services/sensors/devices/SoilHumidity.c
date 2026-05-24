#include "main.h"
#include "func_config.h"
#include "SoilHumidity.h"

// 计算土壤湿度百分比
static void _soil_humidity_calc(SoilHumidity_Handle *handle) {
    float voltage = (float)handle->raw_data / 4095.0f * 3.3f;
    if (handle->calibrated) {      
        // 典型的LM393土壤湿度传感器：干燥时ADC值高，湿润时ADC值低
        if (voltage >= handle->calib_dry) {
            handle->humidity = 0.0f;
        } else if (voltage <= handle->calib_wet) {
            handle->humidity = 100.0f;
        } else {
            handle->humidity = (handle->calib_dry - voltage) /
                               (handle->calib_dry - handle->calib_wet) * 100.0f;
        }
    } else {
        // 未校准时直接输出原始ADC值的百分比
        handle->humidity = voltage;
    }
    handle->data_flag = 1;
}

// 初始化
int soil_humidity_init(SoilHumidity_Handle *handle) {
    if (!handle || !handle->hadc) return -1;

    handle->status = SOIL_HUMIDITY_CALIBRATE;
    handle->raw_data = 0;
    handle->data_flag = 0;
    handle->retry_count = 0;
    handle->start_time = 0;
    handle->humidity = 0.0f;

    soil_humidity_set_calib(handle, SOIL_HUMIDITY_DRY, SOIL_HUMIDITY_WET);

    return 0;
}

// 设置校准值
int soil_humidity_set_calib(SoilHumidity_Handle *handle, float dry, float wet) {
    if (!handle) return -1;

    handle->calib_dry = dry;
    handle->calib_wet = wet;
    handle->calibrated = 1;
    return 0;
}

// 获取已准备好的数据
int soil_humidity_get(SoilHumidity_Handle *handle, float *humidity) {
    if (!handle || !humidity) return -1;
    if (handle->data_flag != 1) return -2;

    *humidity = handle->humidity;
    handle->data_flag = 0;
    return 0;
}

// 状态机驱动
void soil_humidity_run(SoilHumidity_Handle *handle) {
    if (!handle) return;

    // 重试次数达到上限，重置
    if (handle->retry_count >= SOIL_HUMIDITY_MAX_RETRY_COUNT) {
        handle->status = SOIL_HUMIDITY_CALIBRATE;
        handle->retry_count = 0;
        handle->data_flag = 0;
        return;
    }

    switch (handle->status) {

        case SOIL_HUMIDITY_CALIBRATE: {
            uint16_t samples[SOIL_HUMIDITY_SAMPLE_COUNT];

            for (uint8_t n = 0; n < SOIL_HUMIDITY_SAMPLE_COUNT; n++) {
                if (HAL_ADC_Start(handle->hadc) != HAL_OK) {
                    handle->retry_count++;
                    handle->status = SOIL_HUMIDITY_CALIBRATE;
                    return;
                }
                if (HAL_ADC_PollForConversion(handle->hadc, 100) != HAL_OK) {
                    HAL_ADC_Stop(handle->hadc);
                    handle->retry_count++;
                    handle->status = SOIL_HUMIDITY_CALIBRATE;
                    return;
                }
                samples[n] = HAL_ADC_GetValue(handle->hadc);
                HAL_ADC_Stop(handle->hadc);
            }

            // 中值滤波：排序后去掉两端极值
            for (uint8_t i = 0; i < SOIL_HUMIDITY_SAMPLE_COUNT - 1; i++) {
                for (uint8_t j = i + 1; j < SOIL_HUMIDITY_SAMPLE_COUNT; j++) {
                    if (samples[i] > samples[j]) {
                        uint16_t tmp = samples[i];
                        samples[i] = samples[j];
                        samples[j] = tmp;
                    }
                }
            }

            uint32_t acc = 0;
            for (uint8_t i = 2; i < SOIL_HUMIDITY_SAMPLE_COUNT - 2; i++) {
                acc += samples[i];
            }
            
            handle->raw_data = (uint16_t)(acc / (SOIL_HUMIDITY_SAMPLE_COUNT - 4));
            _soil_humidity_calc(handle);
            handle->status = SOIL_HUMIDITY_COMPLETE;
            handle->retry_count = 0;
            break;
        }

        case SOIL_HUMIDITY_COMPLETE:
            if (handle->data_flag == 0) {
                handle->status = SOIL_HUMIDITY_CALIBRATE;
            }
            break;

        default:
            handle->status = SOIL_HUMIDITY_CALIBRATE;
            break;
    }
}

inline int soil_humidity_ready(SoilHumidity_Handle *handle) {
    if (!handle) return -1;
    return (handle->data_flag == 1) ? 0 : -2; // 0: ready, -2: no data
}