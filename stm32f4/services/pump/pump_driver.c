#include "pump_driver.h"

// 底层实现：初始化
static int _pump_init(pump_dev_t *dev) {
    if (!dev || !dev->htim) return -1;
    
    // 启动PWM输出
    HAL_TIM_PWM_Start(dev->htim, dev->channel);
    
    // 初始化状态：默认关闭 (占空比设为0)
    __HAL_TIM_SET_COMPARE(dev->htim, dev->channel, 0);
    dev->current_speed = 0;
    
    return 0;
}

// 底层实现：开启 (默认以100%满载开启)
static int _pump_on(pump_dev_t *dev) {
    if (!dev) return -1;
    return dev->ops->set_speed(dev, 100); // 复用设置速度函数
}

// 底层实现：关闭
static int _pump_off(pump_dev_t *dev) {
    if (!dev) return -1;
    return dev->ops->set_speed(dev, 0);   // 复用设置速度函数，设为0即关闭
}

// 底层实现：PWM调速核心
static int _pump_set_speed(pump_dev_t *dev, uint8_t percent) {
    if (!dev || !dev->htim) return -1;
    
    // 1. 参数限幅保护
    if (percent > 100) {
        percent = 100;
    }
    
    // 2. 计算比较寄存器(CCR)的值
    // 公式：CCR = (目标百分比 / 100) * ARR (自动重装载值)
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(dev->htim);
    uint32_t ccr = (arr * percent) / 100;
    
    // 3. 设置占空比
    __HAL_TIM_SET_COMPARE(dev->htim, dev->channel, ccr);
    
    // 4. 更新状态
    dev->current_speed = percent;
    
    return 0;
}

// 静态的VTable实例（放在Flash中）
static const pump_ops_t pump_vtable = {
    .init = _pump_init,
    .on = _pump_on,
    .off = _pump_off,
    .set_speed = _pump_set_speed,
};

// 设备实例内存分配
static pump_dev_t pump_instance;

// 构造函数：绑定硬件与VTable
pump_dev_t* Pump_Create(TIM_HandleTypeDef *htim, uint32_t channel) {
    pump_instance.ops = &pump_vtable;
    pump_instance.htim = htim;
    pump_instance.channel = channel;
    pump_instance.current_speed = 0;
    
    return &pump_instance;
}
