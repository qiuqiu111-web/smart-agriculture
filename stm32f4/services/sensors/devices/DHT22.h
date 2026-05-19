/* DHT22.h - DHT22 sensor driver public types and API */
#ifndef DHT22_H
#define DHT22_H

#include <stdint.h>
#include "func_config.h"

typedef enum {
    DHT22_EMPTY = 0,
    DHT22_READY,
    DHT22_WAIT,
    DHT22_RECEIVE,
    DHT22_CHECK,
    DHT22_COMPLETE,
} eDHT22_status;

typedef enum {
    DHT22_OK = 0,
    DHT22_ERROR_TIMEOUT,
    DHT22_ERROR_CHECKSUM,
    DHT22_ERROR_DATA,
} eDHT22_error_code;

#define DHT22_MAX_RETRY 3

typedef struct {
    GPIO_handle gpio;      // GPIO 配置
    eDHT22_status status;  // 当前状态
    uint8_t buffer[5];     // 接收的 5 字节数据
    uint32_t start_time;   // 时间戳（us）

    uint8_t data_flag;     // 数据准备好标志
    uint8_t retry_count;   // 连续失败计数
    float temperature;     // 温度（℃）
    float humidity;        // 湿度（%RH）
} DHT22_Handle;

// 初始化，返回 0 成功
int dht22_init(DHT22_Handle *handle);

// 状态机驱动，非阻塞，需定期调用
void dht22_run(DHT22_Handle *handle);

// 数据就绪检查，返回 0 表示数据准备好
int dht22_ready(DHT22_Handle *handle);

// 获取已准备好的数据（若无数据返回非 0）
int dht22_get(DHT22_Handle *handle, float *temperature, float *humidity);

#endif // DHT22_H