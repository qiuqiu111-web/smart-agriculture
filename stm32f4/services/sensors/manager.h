#ifndef __SENSOR_MANAGE_H
#define __SENSOR_MANAGE_H

#include <stdint.h>

#define MAX_SENSORS 5


typedef enum {
    SENSOR_DS3231,
    SENSOR_DHT22,
    SENSOR_DS18B20,
    SENSOR_SOIL_HUMIDITY,
    SENSOR_GY30,
    SENSOR_AHT30
} eSensorsType;

typedef struct {
    float temperature;      // 空气温度（AHT30）
    float humidity;         // 空气湿度（AHT30）
    float soil_temperature; // 土壤温度（DS18B20）
    float soil_humidity;    // 土壤湿度
    float illuminance;      // 光照强度
} Sensors_Data;

typedef struct {
    int (*init)(void *handle); // 初始化函数指针
    void (*run)(void *handle);  // 状态机驱动函数指针
    int (*if_ready)(void *handle); // 数据就绪检查函数指针，返回0表示数据准备好
    int (*get_data)(void *handle, Sensors_Data *data); // 获取数据函数指针，data为输出参数
} Sensors_Ops;

typedef struct {
    eSensorsType type; // 传感器类型
    void *handle;     // 传感器句柄指针 
    const Sensors_Ops *ops;   // 传感器操作函数指针
} Sensor_Example;

typedef struct {
    int sensor_count; // 传感器数量
    Sensor_Example sensors[MAX_SENSORS]; // 传感器数组，最多支持5个传感器
} Sensors_Manager;

// 传感器管理器接口
// 返回0表示成功，返回-1管理器句柄无效，返回-2传感器初始化失败
int Sensors_Manager_Init(Sensors_Manager *manager);

// 运行状态机，非阻塞，需定期调用
void Sensors_Manager_Run(Sensors_Manager *manager);

// 获取已准备好的数据，返回0表示成功，非0表示无数据或错误
int Sensors_Manager_Get_Data(Sensors_Manager *manager, Sensors_Data *data);

#endif // __SENSOR_MANAGE_H