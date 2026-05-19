/* GY30.h - GY30 (BH1750) light sensor driver public types and API */
#ifndef GY30_H
#define GY30_H

#include <stdint.h>
#include "func_config.h"

// GY30 I2C地址（7位地址，HAL库会自动左移1位）
#define GY30_I2C_ADDR1  0x23  // ADDR引脚低电平（7位地址：0100011）
#define GY30_I2C_ADDR2  0x5C  // ADDR引脚高电平（7位地址：1011100）

// GY30命令字
#define GY30_POWER_DOWN     0x00  // 断电
#define GY30_POWER_ON       0x01  // 上电
#define GY30_RESET          0x07  // 重置（仅在上电后有效）
#define GY30_CONT_HR_MODE   0x10  // 连续高分辨率模式（1 lx分辨率）
#define GY30_CONT_HR_MODE2  0x11  // 连续高分辨率模式2（0.5 lx分辨率）
#define GY30_CONT_LR_MODE   0x13  // 连续低分辨率模式（4 lx分辨率）
#define GY30_ONE_HR_MODE    0x20  // 一次高分辨率模式
#define GY30_ONE_HR_MODE2   0x21  // 一次高分辨率模式2
#define GY30_ONE_LR_MODE    0x23  // 一次低分辨率模式

// 默认测量模式
#define GY30_DEFAULT_MODE   GY30_CONT_HR_MODE

// 测量时间（毫秒），根据模式不同
#define GY30_MEASURE_TIME_HR   120U  // 高分辨率模式测量时间（约120ms）
#define GY30_MEASURE_TIME_LR   16U   // 低分辨率模式测量时间（约16ms）

// 最大重试次数
#define GY30_MAX_RETRY_COUNT   3U    // 最大重试次数

typedef enum {
    GY30_EMPTY = 0,      // 初始状态，未初始化
    GY30_READY,          // 已初始化，准备开始测量
    GY30_START,          // 已发送启动测量命令
    GY30_READ,           // 读取测量数据
    GY30_COMPLETE,       // 数据就绪
} eGY30_status;

typedef enum {
    GY30_OK = 0,          // 操作成功
    GY30_ERROR_I2C,       // I2C通信错误
    GY30_ERROR_TIMEOUT,   // 超时错误
    GY30_ERROR_DATA,      // 数据错误（超出范围）
} eGY30_error_code;

typedef struct {
    I2C_handle i2c;           // I2C配置
    eGY30_status status;      // 当前状态
    uint8_t mode;             // 当前测量模式
    uint16_t raw_data;        // 原始测量数据（2字节）
    uint8_t retry_count;      // 重试次数
    uint8_t data_flag;        // 数据准备好标志
    uint32_t start_time;      // 时间戳（ms）
    float illuminance;        // 照度值（lx）
} GY30_Handle;

// 初始化，返回 0 成功
int gy30_init(GY30_Handle *handle);

// 状态机驱动，非阻塞，需定期调用
void gy30_run(GY30_Handle *handle);

// 设置测量模式，返回 0 成功
int gy30_set_mode(GY30_Handle *handle, uint8_t mode);

// 数据就绪检查，返回 0 表示数据准备好
int gy30_ready(GY30_Handle *handle);

// 获取已准备好的数据（若无数据返回非 0）
int gy30_get(GY30_Handle *handle, float *illuminance);

#endif // GY30_H