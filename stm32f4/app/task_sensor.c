#include "cmsis_os.h"
#include "manager.h"
#include "queues.h"
#include "app_config.h"
#include <assert.h>


void GetSensorsDataTask(void const *argument) {
    Sensors_Data sensor_data = {0}, sensor_data_buffer = {0};
    Sensors_Manager manager;

    // 传感器管理器初始化
    assert(Sensors_Manager_Init(&manager) == 0);

    while (1) {
        Sensors_Manager_Run(&manager);
        if (Sensors_Manager_Get_Data(&manager, &sensor_data_buffer) == 0) {
            sensor_data.temperature = sensor_data_buffer.temperature*0.4 + sensor_data.temperature*0.6;
            sensor_data.humidity = sensor_data_buffer.humidity*0.4 + sensor_data.humidity*0.6;
            sensor_data.soil_temperature = sensor_data_buffer.soil_temperature*0.4 + sensor_data.soil_temperature*0.6;
            sensor_data.soil_humidity = sensor_data_buffer.soil_humidity*0.4 + sensor_data.soil_humidity*0.6;
            sensor_data.illuminance = sensor_data_buffer.illuminance*0.4 + sensor_data.illuminance*0.6;
            if (xQueueSend(sensorsDataQueue, &sensor_data,pdMS_TO_TICKS(100)) != pdPASS) {
                Sensors_Data discarded_data;
                xQueueReceive(sensorsDataQueue, &discarded_data, 0);
                xQueueSend(sensorsDataQueue, &sensor_data, 0);
            }
            vTaskDelay(SENSOR_SAMPLE_INTERVAL_MS);
        }
    }
}
