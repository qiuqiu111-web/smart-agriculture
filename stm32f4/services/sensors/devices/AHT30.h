/* AHT30.h - AHT30 I2C temperature & humidity sensor driver */
#ifndef AHT30_H
#define AHT30_H

#include <stdint.h>
#include "func_config.h"

#define AHT30_I2C_ADDR  0x38  // 7-bit I2C address

#define AHT30_MAX_RETRY 3

typedef enum {
    AHT30_READY = 0,
    AHT30_START,
    AHT30_COMPLETE,
} eAHT30_status;

typedef enum {
    AHT30_OK = 0,
    AHT30_ERROR_I2C,
    AHT30_ERROR_CRC,
    AHT30_ERROR_DATA,
} eAHT30_error_code;

typedef struct {
    I2C_handle i2c;
    eAHT30_status status;
    uint8_t retry_count;
    uint8_t data_flag;
    uint32_t start_time;
    float temperature;
    float humidity;
} AHT30_Handle;

int aht30_init(AHT30_Handle *handle);
void aht30_run(AHT30_Handle *handle);
int aht30_ready(AHT30_Handle *handle);
int aht30_get(AHT30_Handle *handle, float *temperature, float *humidity);

#endif // AHT30_H
