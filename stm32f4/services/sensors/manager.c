#include "manager.h"
#include "adapters.h"
#include <stdint.h>

// 此处添加其他传感器的头文件-----------------------------------------
#include "i2c.h" 
#include "DHT22.h"  // 空气温湿度传感器，获取温度和湿度，单总线协议
#include "DS18B20.h"    // 土壤温度传感器，获取土壤温度，单总线协议
#include "SoilHumidity.h" // 土壤湿度传感器，获取土壤湿度，ADC协议
#include "GY30.h" // 光照强度传感器，获取光照强度，I2C协议
#include "AHT30.h" // 空气温湿度传感器，获取温度和湿度，I2C协议
// -----------------------------------------------------------------

// 实例化传感器句柄（需用户根据实际情况修改）-----------------------------
// static DS3231_Handle ds3231_handle;
// static DHT22_Handle dht22_handle;
static DS18B20_Handle ds18b20_handle;
static SoilHumidity_Handle soil_humidity_handle;
static GY30_Handle gy30_handle;
static AHT30_Handle aht30_handle;

// -----------------------------------------------------------------

// 定义传感器类型和句柄的数组
typedef struct {
    eSensorsType type;
    void *handle;
} Sensor_Config;

// 传感器操作函数定义-----------------------------------------
static const Sensors_Ops dht22_ops = {
    .init = (int (*)(void *))dht22_init,
    .run = (void (*)(void *))dht22_run,
    .if_ready = (int (*)(void *))dht22_ready,
    .get_data = dht22_get_adapter, // 适配器函数
};

static const Sensors_Ops ds18b20_ops = {
    .init = (int (*)(void *))ds18b20_init,
    .run = (void (*)(void *))ds18b20_run,
    .if_ready = (int (*)(void *))ds18b20_ready,
    .get_data = ds18b20_get_adapter, // 适配器函数
};

static const Sensors_Ops soil_humidity_ops = {
    .init = (int (*)(void *))soil_humidity_init,
    .run = (void (*)(void *))soil_humidity_run,
    .if_ready = (int (*)(void *))soil_humidity_ready,
    .get_data = soil_humidity_get_adapter, // 适配器函数
};

static const Sensors_Ops gy30_ops = {
    .init = (int (*)(void *))gy30_init,
    .run = (void (*)(void *))gy30_run,
    .if_ready = (int (*)(void *))gy30_ready,
    .get_data = gy30_get_adapter, // 适配器函数
};

static const Sensors_Ops aht30_ops = {
    .init = (int (*)(void *))aht30_init,
    .run = (void (*)(void *))aht30_run,
    .if_ready = (int (*)(void *))aht30_ready,
    .get_data = aht30_get_adapter, // 适配器函数
};

static int _add_sensor(Sensors_Manager *manager, eSensorsType type, void *handle) {
    if (manager->sensor_count >= MAX_SENSORS) return -1; // 超过最大传感器数量
    if (!handle) return -1; // 句柄不能为空

    // 根据传感器类型设置对应的操作函数
    switch (type) {
            break;
        case SENSOR_DHT22:
            manager->sensors[manager->sensor_count] = (Sensor_Example) {
                .type = SENSOR_DHT22,
                .handle = (DHT22_Handle*)handle,
                .ops = &dht22_ops,
            };
            break;
        case SENSOR_DS18B20:
            manager->sensors[manager->sensor_count] = (Sensor_Example) {
                .type = SENSOR_DS18B20,
                .handle = (DS18B20_Handle*)handle,
                .ops = &ds18b20_ops,
            };
            break;
        case SENSOR_SOIL_HUMIDITY:
            manager->sensors[manager->sensor_count] = (Sensor_Example) {
                .type = SENSOR_SOIL_HUMIDITY,
                .handle = (SoilHumidity_Handle*)handle,
                .ops = &soil_humidity_ops,
            };
            break;
        case SENSOR_GY30:
            manager->sensors[manager->sensor_count] = (Sensor_Example) {
                .type = SENSOR_GY30,
                .handle = (GY30_Handle*)handle,
                .ops = &gy30_ops,
            };
            break;
        case SENSOR_AHT30:
            manager->sensors[manager->sensor_count] = (Sensor_Example) {
                .type = SENSOR_AHT30,
                .handle = (AHT30_Handle*)handle,
                .ops = &aht30_ops,
            };
            break;
        default:
            return -1; // 不支持的传感器类型
    }
    manager->sensor_count++;

    return 0;
}

int Sensors_Manager_Init(Sensors_Manager *manager) {
    if (!manager) return -1;
    
    Sensor_Config sensor_configs[] = {
    {SENSOR_DS18B20, &ds18b20_handle},
    {SENSOR_SOIL_HUMIDITY, &soil_humidity_handle},
    {SENSOR_GY30, &gy30_handle},
    {SENSOR_AHT30, &aht30_handle},
    };

    // 配置各个传感器的GPIO和I2C等硬件参数（需用户根据实际情况修改）-----------------
    ds18b20_handle.gpio.port = GPIOD;
    ds18b20_handle.gpio.pin = GPIO_PIN_4;

    soil_humidity_handle.hadc = &hadc1;

    gy30_handle.i2c.hi2c = &hi2c2;
    gy30_handle.i2c.dev_addr = GY30_I2C_ADDR1<<1;

    aht30_handle.i2c.hi2c = &hi2c3;
    aht30_handle.i2c.dev_addr = AHT30_I2C_ADDR<<1;
    // -----------------------------------------------------------------

    manager->sensor_count = 0;
    int i;
    // 添加传感器实例到管理器
    for (int i = 0; i < (int)(sizeof(sensor_configs) / sizeof(Sensor_Config)); i++) {
        if (_add_sensor(manager, sensor_configs[i].type, sensor_configs[i].handle) != 0) {
            return -2;
        }
    }

    // 传感器初始化
    for (i = 0; i < manager->sensor_count; i++) {
        Sensor_Example *sensor = &manager->sensors[i];
        if (sensor->ops->init(sensor->handle) != 0) {
            return -3;
        }
    }

    return 0;
}

void Sensors_Manager_Run(Sensors_Manager *manager) {
    if (!manager) return;

    for (int i = 0; i < manager->sensor_count; i++) {
        Sensor_Example *sensor = &manager->sensors[i];
        sensor->ops->run(sensor->handle);
    }
}

int Sensors_Manager_Get_Data(Sensors_Manager *manager, Sensors_Data *data) {
    if (!manager || !data) return -1;

    // 检查数据标志，确保数据准备好
    for (int i = 0; i < manager->sensor_count; i++) {
        Sensor_Example *sensor = &manager->sensors[i];
        if (sensor->ops->if_ready(sensor->handle) != 0) {
            return -2; // 数据未准备好
        }
    }

    // 获取每个传感器的数据
    for (int i = 0; i < manager->sensor_count; i++) {
        Sensor_Example *sensor = &manager->sensors[i];
        if (sensor->ops->get_data(sensor->handle, data) != 0) {
            return -3; // 获取数据失败
        }
    }

    return 0; // 成功获取数据
}