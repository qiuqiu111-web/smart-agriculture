#ifndef UART_HAL_H
#define UART_HAL_H

#include <stdint.h>
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "frame_codec.h"
#include "app_config.h"

// ---------- 发送 ----------
int UART_Comm_Send(Sensors_Data *data, UART_HandleTypeDef *huart);

// ---------- 接收 ----------
void UART_Comm_StartReceive(UART_HandleTypeDef *huart, TaskHandle_t task_handle);
void UART_Comm_IdleHandler(UART_HandleTypeDef *huart);
int  UART_Comm_Receive(UART_Command *command, UART_HandleTypeDef *huart);

#endif // UART_HAL_H
