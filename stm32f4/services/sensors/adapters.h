/* Sensor_Adapters.h - adapters to unify various driver get() into Sensors_Data */
#ifndef SENSOR_ADAPTERS_H
#define SENSOR_ADAPTERS_H

#include "manager.h"

int ds18b20_get_adapter(void *handle, Sensors_Data *data);
int dht22_get_adapter(void *handle, Sensors_Data *data);
int soil_humidity_get_adapter(void *handle, Sensors_Data *data);
int gy30_get_adapter(void *handle, Sensors_Data *data);
int ds3231_get_adapter(void *handle, Sensors_Data *data);
int aht30_get_adapter(void *handle, Sensors_Data *data);

#endif // SENSOR_ADAPTERS_H
