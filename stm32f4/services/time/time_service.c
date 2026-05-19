#include "stm32f4xx.h"

#define F_CPU SystemCoreClock // 定义CPU频率（Hz），根据实际情况修改
#define CYCNT  DWT->CYCCNT    // DWT周期计数器寄存器地址

// 启用并清零 DWT 周期计数器（在使用 DWT->CYCCNT 之前必须调用）
void DWT_Init(void) {
    // 使能调试跟踪功能（TRC）
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }

#if defined(DWT_LAR)
    // 部分 Cortex-M 芯片需要写入 LAR 解锁 DWT
    DWT->LAR = 0xC5ACCE55;
#endif

    // 清零并使能周期计数器
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t get_current_timestamp_us(void) {
    return (uint32_t)(((uint64_t)CYCNT * 1000000ULL) / F_CPU);
}
uint32_t get_current_timestamp_ms(void) {
    return (uint32_t)(((uint64_t)CYCNT * 1000ULL) / F_CPU);
}

// 延时微秒（忙等待）
// void my_delay_us(uint32_t us) {
//     uint32_t start = get_current_timestamp_us();
//     while (get_current_timestamp_us() - start < us) {
//     }
// }

void my_delay_us(uint32_t us) {
    uint32_t start = CYCNT;
    uint32_t ticks = (uint32_t)(((uint64_t)F_CPU * us) / 1000000ULL);
    while ((CYCNT - start) < ticks);
}