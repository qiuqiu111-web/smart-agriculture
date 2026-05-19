#include "main.h"
#include "DS18B20.h"

// ============ GPIO 模式切换 ============
static void _output_mode(DS18B20_Handle *handle) {
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = handle->gpio.pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(handle->gpio.port, &gpio);
}

static void _input_mode(DS18B20_Handle *handle) {
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = handle->gpio.pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(handle->gpio.port, &gpio);
}

// ============ 1-Wire 复位 ============
static int _reset(DS18B20_Handle *handle) {
    _output_mode(handle);
    GPIO_SET_LOW(handle->gpio);
    DELAY_US(480);

    _input_mode(handle);
    DELAY_US(60);

    int presence = (GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_RESET);

    // 等待总线回到高电平
    uint32_t t0 = GET_TIME_US();
    while (GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_RESET) {
        if (GET_TIME_US() - t0 > 500U) break;
    }

    return presence ? DS18B20_OK : DS18B20_ERROR_PRESENCE;
}

// ============ 1-Wire 位操作 ============
static void _write_bit(DS18B20_Handle *handle, uint8_t bit) {
    _output_mode(handle);
    GPIO_SET_LOW(handle->gpio);
    DELAY_US(1);

    if (bit) {
        _input_mode(handle);
        DELAY_US(60);
    } else {
        DELAY_US(60);
        _input_mode(handle);
        DELAY_US(1);
    }
}

static uint8_t _read_bit(DS18B20_Handle *handle) {
    _output_mode(handle);
    GPIO_SET_LOW(handle->gpio);
    DELAY_US(1);

    _input_mode(handle);
    DELAY_US(10);

    uint8_t bit = (GPIO_READ_LEVEL(handle->gpio) == GPIO_PIN_SET) ? 1 : 0;

    DELAY_US(50);
    return bit;
}

static void _write_byte(DS18B20_Handle *handle, uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        _write_bit(handle, byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t _read_byte(DS18B20_Handle *handle) {
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;
        if (_read_bit(handle)) byte |= 0x80;
    }
    return byte;
}

// ============ CRC8 ============
static uint8_t _crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

// ============ 初始化 ============
int ds18b20_init(DS18B20_Handle *handle) {
    if (!handle || handle->gpio.port == NULL) return -1;

    handle->status      = DS18B20_START;
    handle->data_flag   = 0;
    handle->temperature = 0.0f;
    handle->start_time  = 0;

    _input_mode(handle);
    return _reset(handle);
}

// ============ 非阻塞状态机 ============
void ds18b20_run(DS18B20_Handle *handle) {
    if (!handle) return;

    switch (handle->status) {

        case DS18B20_START:
            // 复位 + 跳过 ROM + 启动温度转换
            if (_reset(handle) != DS18B20_OK) break;
            _write_byte(handle, DS18B20_CMD_SKIP_ROM);
            _write_byte(handle, DS18B20_CMD_CONVERT_T);
            handle->start_time = GET_TIME_MS();
            handle->status = DS18B20_WAIT;
            break;

        case DS18B20_WAIT:
            // 等待 750ms 转换完成
            if (GET_TIME_MS() - handle->start_time >= DS18B20_CONV_TIME_MS) {
                handle->status = DS18B20_READ;
            }
            break;

        case DS18B20_READ:
            // 复位 + 跳过 ROM + 读暂存器 + CRC + 处理
            if (_reset(handle) != DS18B20_OK) {
                handle->status = DS18B20_START;
                break;
            }
            _write_byte(handle, DS18B20_CMD_SKIP_ROM);
            _write_byte(handle, DS18B20_CMD_READ_SCRATCH);

            for (uint8_t i = 0; i < 9; i++) {
                handle->scratchpad[i] = _read_byte(handle);
            }

            if (_crc8(handle->scratchpad, 8) != handle->scratchpad[8]) {
                handle->status = DS18B20_START;
                break;
            }

            {
                int16_t raw = (int16_t)(handle->scratchpad[0] |
                               ((uint16_t)handle->scratchpad[1] << 8));
                handle->temperature = (float)raw * DS18B20_LSB_12BIT;
            }

            if (handle->temperature < -55.0f || handle->temperature > 125.0f) {
                handle->status = DS18B20_START;
                break;
            }

            handle->data_flag = 1;
            handle->status = DS18B20_COMPLETE;
            break;

        case DS18B20_COMPLETE:
            if (handle->data_flag == 0) {
                handle->status = DS18B20_START;
            }
            break;

        default:
            handle->status = DS18B20_START;
            break;
    }
}

// ============ 数据获取 ============
int ds18b20_get(DS18B20_Handle *handle, float *temperature) {
    if (!handle || !temperature) return -1;
    if (handle->data_flag != 1) return -2;

    *temperature = handle->temperature;
    handle->data_flag = 0;
    return 0;
}

int ds18b20_ready(DS18B20_Handle *handle) {
    if (!handle) return -1;
    return (handle->data_flag == 1) ? 0 : -2;
}
