#include "main.h" 
#include "DHT22.h"

// 读数据时的序列状态，防止卡死（内部使用）
typedef enum {
    STATE_DONE = 0,
    STATE_WAIT_HIGH,
    STATE_MEASURE_HIGH,
} eBitReadState;

// 切换输入输出
static void _dht22_output_mode(DHT22_Handle *handle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = handle->gpio.pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出模式
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // 高速
    HAL_GPIO_Init(handle->gpio.port, &GPIO_InitStruct);
}

static void _dht22_input_mode(DHT22_Handle *handle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = handle->gpio.pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // 输入模式
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 上拉
    HAL_GPIO_Init(handle->gpio.port, &GPIO_InitStruct);
}

// 读取一位数据，返回 0 或 1，超时返回 0xFF
static uint8_t _dht22_read_bit(DHT22_Handle *handle) {
    uint32_t start_high = 0, end_high = 0;
    uint32_t t0 = GET_TIME_US();
    uint8_t pin_level;
    eBitReadState state = STATE_WAIT_HIGH;

    // 等待并测量高电平宽度，带超时保护
    while (state != STATE_DONE) {
        pin_level = GPIO_READ_LEVEL(handle->gpio);

        // 超时保护：如果从函数入口超过 10 ms，退出并返回 0xff 表示错误
        if ((GET_TIME_US() - t0) > 10000U) {
            return 0xFF; // 超时错误
        }

        switch (state) {
            case STATE_WAIT_HIGH:
                if (pin_level == GPIO_PIN_SET) {
                    start_high = GET_TIME_US(); // 记录拉高开始时间
                    state = STATE_MEASURE_HIGH;
                }
                break;
            case STATE_MEASURE_HIGH:
                if (pin_level == GPIO_PIN_RESET) {
                    end_high = GET_TIME_US(); // 记录拉高结束时间
                    state = STATE_DONE;
                }
                break;
            default:
                break;
        }
    }

    uint32_t high_us = (end_high > start_high) ? (end_high - start_high) : 0; // 防止计数器溢出导致的负值
    // DHT22: 高电平约 26-28us 表示 0，约 70us 表示 1。取阈值 50us
    return (high_us > 50U) ? 1 : 0;
}

static eDHT22_error_code _dht22_receive_data(DHT22_Handle *handle) {
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t byte = 0;
        for (uint8_t j = 0; j < 8; j++) {
            byte <<= 1;
            uint8_t b = _dht22_read_bit(handle); // 读取每一位
            if (b == 0xFF) { // 超时/错误
                return DHT22_ERROR_TIMEOUT;
            }
            byte |= b;
        }
        handle->buffer[i] = byte; // 存储接收的字节
    }
    return DHT22_OK;
}

static eDHT22_error_code _dht22_check_sum_data(DHT22_Handle *handle) {
    uint32_t checksum = handle->buffer[0] + handle->buffer[1] + handle->buffer[2] + handle->buffer[3];
    if (checksum != handle->buffer[4]) {
        return DHT22_ERROR_CHECKSUM; // 校验错误
    }
    return DHT22_OK; // 数据校验成功
}

static void _dht22_process_data(DHT22_Handle *handle) {
    // 处理温湿度数据，转换为实际值
    uint16_t raw_humidity = ((uint16_t)handle->buffer[0] << 8) | (uint16_t)handle->buffer[1];
    uint16_t raw_temp = ((uint16_t)handle->buffer[2] << 8) | (uint16_t)handle->buffer[3];

    handle->humidity = raw_humidity / 10.0f; // 湿度值，单位%
    // 处理负数情况
    if (raw_temp & 0x8000U) {
        raw_temp &= 0x7FFFU;
        handle->temperature = -((float)raw_temp / 10.0f);
    } else {
        handle->temperature = (float)raw_temp / 10.0f;
    }
    handle->data_flag = 1; // 数据准备好
}

static eDHT22_error_code _dht22_check_data(DHT22_Handle *handle) {
    if (handle->humidity < 0.0f || handle->humidity > 100.0f || handle->temperature < -40.0f || handle->temperature > 80.0f) {
        handle->data_flag = 0; // 数据异常，标志清零
        return DHT22_ERROR_DATA; // 数据错误
    }
    return DHT22_OK; // 数据正常
}

int dht22_get(DHT22_Handle *handle, float *temperature, float *humidity) {
    if (!handle || !temperature || !humidity) return -1;
    if (handle->data_flag != 1) return -2; // no data
    *temperature = handle->temperature;
    *humidity = handle->humidity;
    handle->data_flag = 0; // 清除标志位
    handle->status = DHT22_EMPTY;
    return 0;
}

// DHT22初始化函数
int dht22_init(DHT22_Handle *handle) {
    if (!handle || handle->gpio.port == NULL) return -1;
    handle->retry_count = 0;
    _dht22_input_mode(handle);
    if(GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_SET) {
        handle->status = DHT22_EMPTY;
        return 0; // GPIO初始化成功
    }
    handle->status = DHT22_EMPTY;
    return -1; // GPIO初始化失败
}

// DHT22状态切换函数
void dht22_run(DHT22_Handle *handle) {
    if (handle->retry_count >= DHT22_MAX_RETRY) {
        handle->retry_count = 0;
        handle->status = DHT22_EMPTY;
        return;
    }

    switch (handle->status) {
        case DHT22_EMPTY:
            // 上拉电阻信号未占用
            if (GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_SET) {
                _dht22_output_mode(handle);
                handle->status = DHT22_READY; // 进入准备状态
                GPIO_SET_LOW(handle->gpio); // 拉低信号
                handle->start_time = GET_TIME_US(); 
            }
            break;
        case DHT22_READY:
            // 拉低至少800us，通知传感器准备发送数据
                if(GET_TIME_US() - handle->start_time >= 1000) { 
                    _dht22_input_mode(handle); // 切换到输入模式，等待传感器响应
                    handle->status = DHT22_WAIT;
                    handle->start_time = GET_TIME_US(); // 记录响应开始时间
                }
            break;
        case DHT22_WAIT:
            // 等待传感器响应，持续80us
            if(GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_RESET) { // 传感器拉低信号，响应开始
                handle->status = DHT22_RECEIVE; // 进入接收状态
                handle->start_time = GET_TIME_US(); // 记录接收开始时间
            } else if (GET_TIME_US() - handle->start_time >= 100) { // 超过100us未响应，重置状态
                handle->retry_count++;
                handle->status = DHT22_EMPTY;
            }
            break;
        case DHT22_RECEIVE:
            // 接收数据，40位，5字节
            if (_dht22_receive_data(handle) != DHT22_OK) {
                handle->retry_count++;
                handle->status = DHT22_EMPTY;
            } else {
                handle->status = DHT22_CHECK;
            }
            break;
        case DHT22_CHECK:
            // 校验数据，校验失败回到初始状态
            if (_dht22_check_sum_data(handle) == DHT22_OK) {
                _dht22_process_data(handle); // 处理数据，准备好温湿度值
                // 最后检查
                if (_dht22_check_data(handle) == DHT22_OK) {
                    handle->retry_count = 0;
                    handle->status = DHT22_COMPLETE;
                } else {
                    handle->retry_count++;
                    handle->status = DHT22_EMPTY;
                }
            }
            else {
                handle->retry_count++;
                handle->status = DHT22_EMPTY; // 校验失败，重置状态
            }
            break;
        case DHT22_COMPLETE:
            // 若数据接收完成,则flag被清零，重置状态
            if(handle->data_flag != 1) {
                handle->retry_count = 0;
                handle->status = DHT22_EMPTY;
            }
            break;
        default:
            // 无效状态，重置为初始状态
            handle->status = DHT22_EMPTY;
            break;
    }
}

inline int dht22_ready(DHT22_Handle *handle) {
    if (!handle) return -1;
    return (handle->data_flag == 1) ? 0 : -2; // 0: ready, -2: no data, 0是方便统一返回值
}