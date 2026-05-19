#include "queues.h"
#include "app_config.h"

QueueHandle_t sensorsDataQueue;
StaticQueue_t xSensorsDataQueueBuffer;
uint8_t pucSensorsDataQueueStorage[SENSOR_QUEUE_SIZE * sizeof(Sensors_Data)];

QueueHandle_t commandQueue;
StaticQueue_t xCommandQueueBuffer;
uint8_t pucCommandQueueStorage[COMMAND_QUEUE_SIZE * sizeof(UART_Command)];

int QueueInit(void) {
    sensorsDataQueue = xQueueCreateStatic(SENSOR_QUEUE_SIZE, sizeof(Sensors_Data),
                                          pucSensorsDataQueueStorage,
                                          &xSensorsDataQueueBuffer);
    if (sensorsDataQueue == NULL) return -1;

    commandQueue = xQueueCreateStatic(COMMAND_QUEUE_SIZE, sizeof(UART_Command),
                                      pucCommandQueueStorage,
                                      &xCommandQueueBuffer);
    if (commandQueue == NULL) return -1;

    return 0;
}
