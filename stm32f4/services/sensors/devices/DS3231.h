/* DS3231.h - DS3231 real-time clock driver public types and API */
#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "func_config.h"

// DS3231 I2C地址
#define DS3231_I2C_ADDR  0x68  // 7位地址：01101000

// DS3231寄存器地址
#define DS3231_REG_SECONDS     0x00  // 秒（00-59）
#define DS3231_REG_MINUTES     0x01  // 分（00-59）
#define DS3231_REG_HOURS       0x02  // 时（00-23或带AM/PM标志）
#define DS3231_REG_DAY         0x03  // 星期（1-7，1=星期日）
#define DS3231_REG_DATE        0x04  // 日（01-31）
#define DS3231_REG_MONTH       0x05  // 月（01-12）
#define DS3231_REG_YEAR        0x06  // 年（00-99）

// 时间格式标志（小时寄存器）
#define DS3231_HOUR_12H_FORMAT 0x40  // 12小时制标志
#define DS3231_HOUR_PM_FLAG    0x20  // PM标志（12小时制下）
#define DS3231_HOUR_24H_MASK   0x3F  // 24小时制掩码

// 状态机：仅三种状态
typedef enum {
    DS3231_READ,      // 读取时间寄存器（7字节）
    DS3231_PARSE,     // 解析数据并校验
    DS3231_COMPLETE,  // 数据就绪，等待上层读取
} eDS3231_status;

typedef enum {
    DS3231_OK = 0,
    DS3231_ERROR_I2C,
    DS3231_ERROR_DATA,
} eDS3231_error_code;

// 时间数据结构（BCD格式转换后）
typedef struct {
    uint8_t seconds;       // 秒 (0-59)
    uint8_t minutes;       // 分 (0-59)
    uint8_t hours;         // 时 (0-23，24小时制)
    uint8_t day_of_week;   // 星期 (1-7，1=星期日)
    uint8_t date;          // 日 (1-31)
    uint8_t month;         // 月 (1-12)
    uint8_t year;          // 年 (0-99)
    uint8_t century;       // 世纪标志
} DS3231_Time;

// DS3231句柄结构
typedef struct {
    I2C_handle i2c;               // I2C配置
    eDS3231_status status;        // 当前状态
    uint8_t raw_time[7];          // 原始时间数据（7字节）
    DS3231_Time time;             // 解析后的时间
    uint8_t retry_count;          // 重试次数
    uint8_t data_flag;            // 数据准备好标志
} DS3231_Handle;

// 初始化，返回 0 成功
int ds3231_init(DS3231_Handle *handle);

// 状态机驱动，非阻塞，需定期调用
void ds3231_run(DS3231_Handle *handle);

// 获取已准备好的时间数据（若无数据返回非 0）
int ds3231_get_time(DS3231_Handle *handle, DS3231_Time *time);

// 数据就绪检查，返回 0 表示数据准备好
int ds3231_ready(DS3231_Handle *handle);

// 设置时间（仅在初始化时调用一次）
int ds3231_set_time(DS3231_Handle *handle, const DS3231_Time *time);

#endif // DS3231_H
