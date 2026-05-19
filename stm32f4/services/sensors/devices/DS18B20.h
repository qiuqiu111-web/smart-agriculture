/* DS18B20.h - DS18B20 temperature sensor driver (non-blocking) */
#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include "func_config.h"

// DS18B20命令字
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_READ_SCRATCH    0xBE

// 分辨率设置
#define DS18B20_RESOLUTION_9BIT     0x1F
#define DS18B20_RESOLUTION_10BIT    0x3F
#define DS18B20_RESOLUTION_11BIT    0x5F
#define DS18B20_RESOLUTION_12BIT    0x7F
#define DS18B20_DEFAULT_RESOLUTION  DS18B20_RESOLUTION_12BIT

#define DS18B20_LSB_12BIT    0.0625f
#define DS18B20_CONV_TIME_MS 750U

typedef enum {
    DS18B20_START,       // 发复位+跳过ROM+启动转换 → 记录时间 → WAIT
    DS18B20_WAIT,        // 等待 750ms 转换完成 → READ
    DS18B20_READ,        // 发复位+跳过ROM+读暂存器 → CRC校验 → 换算温度 → COMPLETE
    DS18B20_COMPLETE,    // 等待上层消费 data_flag → START
} eDS18B20_status;

typedef enum {
    DS18B20_OK = 0,
    DS18B20_ERROR_PRESENCE,
    DS18B20_ERROR_CRC,
} eDS18B20_error_code;

typedef struct {
    GPIO_handle gpio;
    eDS18B20_status status;
    uint8_t scratchpad[9];
    float temperature;
    uint8_t data_flag;
    uint32_t start_time;
} DS18B20_Handle;

int ds18b20_init(DS18B20_Handle *handle);
void ds18b20_run(DS18B20_Handle *handle);
int ds18b20_get(DS18B20_Handle *handle, float *temperature);
int ds18b20_ready(DS18B20_Handle *handle);

#endif // DS18B20_H
