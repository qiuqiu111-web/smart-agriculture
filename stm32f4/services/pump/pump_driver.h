#ifndef PUMP_DRIVER_H
#define PUMP_DRIVER_H

#include <stdint.h>
#include "tim.h" 

// 前置声明设备结构体
typedef struct pump_dev pump_dev_t;

// --- 虚函数表 (VTable) ---
typedef struct {
    int (*init)(pump_dev_t *dev);
    int (*on)(pump_dev_t *dev);
    int (*off)(pump_dev_t *dev);
    int (*set_speed)(pump_dev_t *dev, uint8_t percent); // 0-100 百分比
} pump_ops_t;

// --- 设备实例结构体 ---
struct pump_dev {
    const pump_ops_t *ops;      // 函数指针表
    TIM_HandleTypeDef *htim;    // 具体使用哪个定时器
    uint32_t channel;           // 具体使用哪个通道 (如 TIM_CHANNEL_1)
    uint8_t current_speed;      // 记录当前速度状态
};

// --- 构造函数 (实例化设备) ---
// 传入硬件句柄，返回一个初始化好的设备指针
pump_dev_t* Pump_Create(TIM_HandleTypeDef *htim, uint32_t channel);

// --- 提供给上层的便捷API (内部通过VTable调用) ---
static inline int Pump_Init(pump_dev_t *dev) { return dev->ops->init(dev); }
static inline int Pump_On(pump_dev_t *dev) { return dev->ops->on(dev); }
static inline int Pump_Off(pump_dev_t *dev) { return dev->ops->off(dev); }
static inline int Pump_SetSpeed(pump_dev_t *dev, uint8_t percent) { return dev->ops->set_speed(dev, percent); }

#endif // PUMP_DRIVER_H
