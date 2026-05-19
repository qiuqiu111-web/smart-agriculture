#ifndef GETTIME_H
#define GETTIME_H

// 获取当前时间戳（微秒）
uint32_t get_current_timestamp_us(void) ;
// 获取当前时间戳（毫秒）
uint32_t get_current_timestamp_ms(void) ;

// 延时微秒
void my_delay_us(uint32_t us) ;

// 启用 DWT 周期计数器
void DWT_Init(void);

#endif // GETTIME_H