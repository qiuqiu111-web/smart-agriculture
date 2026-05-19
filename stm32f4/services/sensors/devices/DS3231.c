#include "main.h"
#include "stm32f4xx_hal_i2c.h"
#include "DS3231.h"

#ifndef I2C_MEMADD_SIZE_8BIT
#define I2C_MEMADD_SIZE_8BIT 1
#endif

// BCD码转十进制
static uint8_t _bcd_to_dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 十进制转BCD码
static uint8_t _dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// 读取DS3231寄存器
static eDS3231_error_code _ds3231_read_registers(DS3231_Handle *handle,
    uint8_t reg_addr, uint8_t *data, uint8_t size) {
    HAL_StatusTypeDef hal_status = I2C_MEM_READ(handle->i2c, reg_addr,
        I2C_MEMADD_SIZE_8BIT, data, size);
    if (hal_status != HAL_OK) {
        return DS3231_ERROR_I2C;
    }
    return DS3231_OK;
}

// 写入DS3231寄存器
static eDS3231_error_code _ds3231_write_registers(DS3231_Handle *handle,
    uint8_t reg_addr, const uint8_t *data, uint8_t size) {
    HAL_StatusTypeDef hal_status = I2C_MEM_WRITE(handle->i2c, reg_addr,
        I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, size);
    if (hal_status != HAL_OK) {
        return DS3231_ERROR_I2C;
    }
    return DS3231_OK;
}

// 解析时间数据（BCD转十进制）
static void _ds3231_parse_time(DS3231_Handle *handle) {
    uint8_t *raw = handle->raw_time;

    handle->time.seconds     = _bcd_to_dec(raw[0] & 0x7F);
    handle->time.minutes     = _bcd_to_dec(raw[1] & 0x7F);

    if (raw[2] & DS3231_HOUR_12H_FORMAT) {
        uint8_t hour12 = _bcd_to_dec(raw[2] & 0x1F);
        if (raw[2] & DS3231_HOUR_PM_FLAG) {
            handle->time.hours = (hour12 == 12) ? 12 : (hour12 + 12);
        } else {
            handle->time.hours = (hour12 == 12) ? 0 : hour12;
        }
    } else {
        handle->time.hours = _bcd_to_dec(raw[2] & DS3231_HOUR_24H_MASK);
    }

    handle->time.day_of_week = _bcd_to_dec(raw[3] & 0x07);
    handle->time.date        = _bcd_to_dec(raw[4] & 0x3F);
    handle->time.month       = _bcd_to_dec(raw[5] & 0x1F);
    handle->time.century     = (raw[5] & 0x80) ? 1 : 0;
    handle->time.year        = _bcd_to_dec(raw[6]);
}

// 检查时间数据是否合理
static eDS3231_error_code _ds3231_check_time(const DS3231_Time *time) {
    if (!time) return DS3231_ERROR_DATA;

    if (time->seconds > 59 || time->minutes > 59 || time->hours > 23 ||
        time->day_of_week < 0 || time->day_of_week > 6 ||
        time->date < 1 || time->date > 31 ||
        time->month < 1 || time->month > 12 ||
        time->year > 99) {
        return DS3231_ERROR_DATA;
    }

    uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (time->date > days_in_month[time->month - 1]) {
        return DS3231_ERROR_DATA;
    }

    return DS3231_OK;
}

// 将时间结构转换为原始寄存器数据
static void _ds3231_time_to_raw(const DS3231_Time *time, uint8_t *raw) {
    raw[0] = _dec_to_bcd(time->seconds) & 0x7F;
    raw[1] = _dec_to_bcd(time->minutes) & 0x7F;
    raw[2] = _dec_to_bcd(time->hours) & DS3231_HOUR_24H_MASK;
    raw[3] = _dec_to_bcd(time->day_of_week) & 0x07;
    raw[4] = _dec_to_bcd(time->date) & 0x3F;
    raw[5] = _dec_to_bcd(time->month) & 0x1F;
    if (time->century) raw[5] |= 0x80;
    raw[6] = _dec_to_bcd(time->year);
}

// 默认初始时间
static const DS3231_Time init_time = {
    .seconds     = 0,
    .minutes     = 0,
    .hours       = 0,
    .day_of_week = 1,
    .date        = 1,
    .month       = 1,
    .year        = 26,
    .century     = 1,
};

// 设置时间（仅在初始化时调用一次）
static int _ds3231_set_time(DS3231_Handle *handle, const DS3231_Time *time) {
    if (!handle || !time) return -1;

    uint8_t raw[7];
    _ds3231_time_to_raw(time, raw);
    return _ds3231_write_registers(handle, DS3231_REG_SECONDS, raw, 7) == DS3231_OK ? 0 : -2;
}

// DS3231初始化函数
int ds3231_init(DS3231_Handle *handle) {
    if (!handle || handle->i2c.hi2c == NULL) return -1;

    handle->status      = DS3231_READ;
    handle->data_flag   = 0;
    handle->retry_count = 0;

    _ds3231_set_time(handle, &init_time);

    return 0;
}

// 获取已准备好的时间数据
int ds3231_get_time(DS3231_Handle *handle, DS3231_Time *time) {
    if (!handle || !time) return -1;
    if (handle->data_flag != 1) return -2;

    *time = handle->time;
    handle->data_flag = 0;
    return 0;
}

// DS3231状态机驱动函数
void ds3231_run(DS3231_Handle *handle) {
    if (!handle) return;

    switch (handle->status) {

        case DS3231_READ:
            if (_ds3231_read_registers(handle, DS3231_REG_SECONDS,
                                       handle->raw_time, 7) == DS3231_OK) {
                handle->status = DS3231_PARSE;
                handle->retry_count = 0;
            } else {
                handle->retry_count++;
            }
            break;

        case DS3231_PARSE:
            _ds3231_parse_time(handle);
            if (_ds3231_check_time(&handle->time) == DS3231_OK) {
                handle->data_flag = 1;
                handle->status = DS3231_COMPLETE;
            } else {
                handle->retry_count++;
                handle->status = DS3231_READ;
            }
            break;

        case DS3231_COMPLETE:
            if (handle->data_flag == 0) {
                handle->status = DS3231_READ;
            }
            break;

        default:
            handle->status = DS3231_READ;
            break;
    }
}

int ds3231_ready(DS3231_Handle *handle) {
    if (!handle) return -1;
    return (handle->data_flag == 1) ? 0 : -2;
}
