#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "cmsis_os.h"

// ============ 任务优先级 ============
#define TASK_PRIO_SENSOR    osPriorityNormal
#define TASK_PRIO_SEND      osPriorityBelowNormal
#define TASK_PRIO_RECV      osPriorityHigh
#define TASK_PRIO_CONTROL   osPriorityAboveNormal

// ============ 任务栈大小 (words) ============
#define STACK_SENSOR        512
#define STACK_SEND          256
#define STACK_RECV          256
#define STACK_CONTROL       256

// ============ 队列长度 ============
#define SENSOR_QUEUE_SIZE   10
#define COMMAND_QUEUE_SIZE  10

// ============ 采样间隔 (ms) ============
#define SENSOR_SAMPLE_INTERVAL_MS  2000

// ============ UART 配置 ============
#define UART_RX_BUF_SIZE    128
#define UART_MAX_RETRY      3

#endif // APP_CONFIG_H
