#include "uart_hal.h"
#include <string.h>

// ============ 发送端 ============
static uint8_t tx_buf[UART_FRAME_SIZE];

int UART_Comm_Send(Sensors_Data *data, UART_HandleTypeDef *huart) {
    if (!data) return -1;

    uint8_t retry_count = 0;

    frame_build_sensor(data, tx_buf);

    while (HAL_UART_Transmit(huart, tx_buf, UART_FRAME_SIZE, 100) != HAL_OK) {
        retry_count++;
        if (retry_count >= UART_MAX_RETRY) {
            return -1;
        }
    }
    return 0;
}

// ============ 接收端 ============
static uint8_t rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t rx_len = 0;
static TaskHandle_t recv_task_handle = NULL;

void UART_Comm_StartReceive(UART_HandleTypeDef *huart, TaskHandle_t task_handle) {
    recv_task_handle = task_handle;
    HAL_UART_Receive_DMA(huart, rx_buf, UART_RX_BUF_SIZE);
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);  // 启用 IDLE 中断
}

void UART_Comm_IdleHandler(UART_HandleTypeDef *huart) {
    if (!__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE)) return;

    __HAL_UART_CLEAR_IDLEFLAG(huart);

    __HAL_DMA_DISABLE(huart->hdmarx);

    rx_len = UART_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);

    __HAL_DMA_CLEAR_FLAG(huart->hdmarx, __HAL_DMA_GET_TC_FLAG_INDEX(huart->hdmarx));

    if (recv_task_handle && rx_len > 0) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(recv_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int UART_Comm_Receive(UART_Command *command, UART_HandleTypeDef *huart) {
    if (!command || rx_len == 0) {
        rx_len = 0;
        huart->RxState = HAL_UART_STATE_READY;
        huart->hdmarx->State = HAL_DMA_STATE_READY;
        HAL_UART_Receive_DMA(huart, rx_buf, UART_RX_BUF_SIZE);
        return -1;
    }

    int found = frame_parse_command(rx_buf, rx_len, command);

    huart->RxState = HAL_UART_STATE_READY;
    huart->hdmarx->State = HAL_DMA_STATE_READY;
    rx_len = 0;
    HAL_UART_Receive_DMA(huart, rx_buf, UART_RX_BUF_SIZE);

    return found;
}
