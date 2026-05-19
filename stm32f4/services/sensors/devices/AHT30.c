#include "AHT30.h"
#include "func_config.h"
#include "i2c.h"
#include "main.h"

// AHT30 命令字
#define AHT30_CMD_INIT      0xBE
#define AHT30_CMD_TRIGGER   0xAC
#define AHT30_INIT_PARAM1   0x08
#define AHT30_INIT_PARAM2   0x00
#define AHT30_TRIG_PARAM1   0x33
#define AHT30_TRIG_PARAM2   0x00

#define AHT30_CALI_BIT      0x08  // 状态字节 bit3 = 校准完成
#define AHT30_BUSY_BIT      0x80  // 状态字节 bit7 = 忙

#define AHT30_MEASURE_TIME  80U   // 测量转换时间（ms）
#define AHT30_INIT_TIMEOUT  500U  // 校准等待超时（ms）

static uint8_t _aht30_crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

static eAHT30_error_code _aht30_read_status(AHT30_Handle *handle, uint8_t *status) {
    HAL_StatusTypeDef hal_status = I2C_MASTER_RECEIVE(handle->i2c, status, 1);
    return (hal_status == HAL_OK) ? AHT30_OK : AHT30_ERROR_I2C;
}

// 发送触发测量命令 [0xAC, 0x33, 0x00]
static eAHT30_error_code _aht30_trigger(AHT30_Handle *handle) {
    uint8_t cmd[3] = {AHT30_CMD_TRIGGER, AHT30_TRIG_PARAM1, AHT30_TRIG_PARAM2};
    HAL_StatusTypeDef hal_status = I2C_MASTER_TRANSMIT(handle->i2c, cmd, 3);
    return (hal_status == HAL_OK) ? AHT30_OK : AHT30_ERROR_I2C;
}

// 读取7字节（状态 + 5字节数据 + CRC），校验CRC，计算温湿度
static eAHT30_error_code _aht30_read_data(AHT30_Handle *handle) {
    uint8_t buf[7] = {0};
    HAL_StatusTypeDef hal_status = I2C_MASTER_RECEIVE(handle->i2c, buf, 7);
    if (hal_status != HAL_OK) return AHT30_ERROR_I2C;

    // CRC校验覆盖前6字节，结果在第7字节
    if (_aht30_crc8(buf, 6) != buf[6]) return AHT30_ERROR_CRC;

    uint32_t hum_raw = ((uint32_t)buf[1] << 12)
                     | ((uint32_t)buf[2] << 4)
                     | ((uint32_t)buf[3] >> 4);

    uint32_t temp_raw = ((uint32_t)(buf[3] & 0x0F) << 16)
                      | ((uint32_t)buf[4] << 8)
                      |  (uint32_t)buf[5];

    handle->humidity    = (float)hum_raw  / 1048576.0f * 100.0f;
    handle->temperature = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;

    return AHT30_OK;
}

static eAHT30_error_code _aht30_check_data(AHT30_Handle *handle) {
    if (handle->humidity < 0.0f || handle->humidity > 100.0f
        || handle->temperature < -40.0f || handle->temperature > 85.0f) {
        handle->data_flag = 0;
        return AHT30_ERROR_DATA;
    }
    return AHT30_OK;
}

int aht30_init(AHT30_Handle *handle) {
    if (!handle || handle->i2c.hi2c == NULL) return -1;

    handle->retry_count = 0;
    handle->data_flag   = 0;

    // 发送校准命令
    uint8_t cmd[3] = {AHT30_CMD_INIT, AHT30_INIT_PARAM1, AHT30_INIT_PARAM2};
    if (I2C_MASTER_TRANSMIT(handle->i2c, cmd, 3) != HAL_OK) return -2;

    // 等待校准完成（状态字节 bit3 置位）
    uint32_t t0 = GET_TIME_MS();
    uint8_t status = 0;
    while (GET_TIME_MS() - t0 < AHT30_INIT_TIMEOUT) {
        if (_aht30_read_status(handle, &status) == AHT30_OK
            && (status & AHT30_CALI_BIT)) {
            handle->status = AHT30_READY;
            return 0;
        }
        DELAY_MS(10);
    }

    handle->status = AHT30_READY;
    return -3; // 校准超时
}

void aht30_run(AHT30_Handle *handle) {
    if (!handle) return;

    if (handle->retry_count >= AHT30_MAX_RETRY) {
        handle->retry_count = 0;
        handle->status = AHT30_READY;
        return;
    }

    switch (handle->status) {
        case AHT30_READY:
            if (_aht30_trigger(handle) == AHT30_OK) {
                handle->start_time = GET_TIME_MS();
                handle->status = AHT30_START;
            } else {
                handle->retry_count++;
            }
            break;

        case AHT30_START:
            if (GET_TIME_MS() - handle->start_time >= AHT30_MEASURE_TIME) {
                if (_aht30_read_data(handle) == AHT30_OK) {
                    if (_aht30_check_data(handle) == AHT30_OK) {
                        handle->data_flag = 1;
                        handle->retry_count = 0;
                        handle->status = AHT30_COMPLETE;
                    } else {
                        handle->retry_count++;
                        handle->status = AHT30_READY;
                    }
                } else {
                    handle->retry_count++;
                    handle->status = AHT30_READY;
                }
            }
            break;

        case AHT30_COMPLETE:
            if (handle->data_flag == 0) {
                handle->status = AHT30_READY;
            }
            break;

        default:
            handle->status = AHT30_READY;
            break;
    }
}

int aht30_ready(AHT30_Handle *handle) {
    if (!handle) return -1;
    return (handle->data_flag == 1) ? 0 : -2;
}

int aht30_get(AHT30_Handle *handle, float *temperature, float *humidity) {
    if (!handle || !temperature || !humidity) return -1;
    if (handle->data_flag != 1) return -2;

    *temperature = handle->temperature;
    *humidity    = handle->humidity;
    handle->data_flag = 0;
    return 0;
}
