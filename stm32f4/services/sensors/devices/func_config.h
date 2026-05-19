#ifndef FUNC_CONFIG_H
#define FUNC_CONFIG_H

#define STM32f4 1
#if STM32f4
#include "stm32f4xx.h" // 包含STM32F4系列的寄存器定义
// #include "stm32f4xx_hal.h" // 包含STM32 HAL库头文件
#include "stm32f4xx_hal_i2c.h" // 包含I2C相关的HAL库头文件
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
}GPIO_handle;

typedef struct {
    I2C_HandleTypeDef *hi2c;  // HAL I2C句柄指针
    uint16_t dev_addr;        // 设备地址（7位，未左移）
} I2C_handle;
#endif

#include <stdint.h>
#include "time_service.h"

typedef enum {
    GPIO_ERROR = 0,
    GPIO_SUCCESS
} eGPIO_status;

// GPIO操作宏
#define GPIO_SET_HIGH(gpio)    HAL_GPIO_WritePin((gpio).port, (gpio).pin, GPIO_PIN_SET)
#define GPIO_SET_LOW(gpio)     HAL_GPIO_WritePin((gpio).port, (gpio).pin, GPIO_PIN_RESET)
#define GPIO_READ_LEVEL(gpio)  HAL_GPIO_ReadPin((gpio).port, (gpio).pin)
#define GPIO_TOGGLE(gpio)      HAL_GPIO_TogglePin((gpio).port, (gpio).pin)

#define I2C_DEFAULT_TIMEOUT    1000U // I2C默认超时时间（ms）
// I2C主设备发送宏
#define I2C_MASTER_TRANSMIT(i2c, data, size) \
    HAL_I2C_Master_Transmit((i2c).hi2c, ((i2c).dev_addr), (uint8_t*)(data), (size), I2C_DEFAULT_TIMEOUT)
#define I2C_MASTER_RECEIVE(i2c, data, size) \
    HAL_I2C_Master_Receive((i2c).hi2c, ((i2c).dev_addr), (uint8_t*)(data), (size), I2C_DEFAULT_TIMEOUT)
#define I2C_MEM_WRITE(i2c, mem_addr, mem_addr_size, data, size) \
    HAL_I2C_Mem_Write((i2c).hi2c, ((i2c).dev_addr), (mem_addr), (mem_addr_size), (uint8_t*)(data), (size), I2C_DEFAULT_TIMEOUT)
#define I2C_MEM_READ(i2c, mem_addr, mem_addr_size, data, size) \
    HAL_I2C_Mem_Read((i2c).hi2c, ((i2c).dev_addr), (mem_addr), (mem_addr_size), (uint8_t*)(data), (size), I2C_DEFAULT_TIMEOUT)
#define I2C_IS_READY(i2c) ((i2c).hi2c->State == HAL_I2C_STATE_READY)

#define GET_TIME_US()          get_current_timestamp_us()
#define GET_TIME_MS()          get_current_timestamp_ms()
#define DELAY_US(us)           my_delay_us(us)
#define DELAY_MS(ms)           HAL_Delay(ms)

#endif // FUNC_CONFIG_H