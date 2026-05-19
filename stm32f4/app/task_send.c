#include "cmsis_os.h"
#include "manager.h"
#include "uart_hal.h"
#include "queues.h"

void SendDataTask(void const *argument) {
    Sensors_Data sensor_data;
    while (1) {
        if (xQueueReceive(sensorsDataQueue, &sensor_data, portMAX_DELAY) == pdPASS) {
            UART_Comm_Send(&sensor_data, &huart1);
        }
    }
}
