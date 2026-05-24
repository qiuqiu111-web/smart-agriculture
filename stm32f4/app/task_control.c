#include "cmsis_os.h"
#include "queues.h"
#include "pump_driver.h"

void ControlCommandTask(void const *argument) {
    UART_Command command;
    pump_dev_t *water_pump = Pump_Create(&htim3, TIM_CHANNEL_1);
    Pump_Init(water_pump);  // 启动 TIM3 PWM 输出

    while (1) {
        if (xQueueReceive(commandQueue, &command, portMAX_DELAY) == pdPASS) {
            if (command.command_id == 0x01 && command.payload == 1) { 
                // 进行一次脉冲浇水，减少决策端的不确定
                Pump_On(water_pump);
                vTaskDelay(pdMS_TO_TICKS(5000)); // 浇水持续5秒
                Pump_Off(water_pump);
            }
        }
    }
}
