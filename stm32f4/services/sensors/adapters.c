#include "adapters.h"
#include "DS18B20.h"
#include "DHT22.h"
#include "SoilHumidity.h"
#include "GY30.h"
#include "DS3231.h"
#include "AHT30.h"

int ds18b20_get_adapter(void *handle, Sensors_Data *data) {
    if (!handle || !data) return -1;
    float temp = 0.0f;
    int ret = ds18b20_get((DS18B20_Handle*)handle, &temp);
    if (ret == 0) data->soil_temperature = temp;
    return ret;
}

int dht22_get_adapter(void *handle, Sensors_Data *data) {
    if (!handle || !data) return -1;
    float temp = 0.0f, hum = 0.0f;
    int ret = dht22_get((DHT22_Handle*)handle, &temp, &hum);
    if (ret == 0) {
        data->temperature = temp;
        data->humidity = hum;
    }
    return ret;
}

int soil_humidity_get_adapter(void *handle, Sensors_Data *data) {
    if (!handle || !data) return -1;
    float hum = 0.0f;
    int ret = soil_humidity_get((SoilHumidity_Handle*)handle, &hum);
    if (ret == 0) data->soil_humidity = hum;
    return ret;
}

int gy30_get_adapter(void *handle, Sensors_Data *data) {
    if (!handle || !data) return -1;
    float lux = 0.0f;
    int ret = gy30_get((GY30_Handle*)handle, &lux);
    if (ret == 0) data->illuminance = lux;
    return ret;
}

int ds3231_get_adapter(void *handle, Sensors_Data *data) {
    // DS3231时间字段已从Sensors_Data移除，此适配器暂不可用
    (void)handle;
    (void)data;
    return -1;
}

int aht30_get_adapter(void *handle, Sensors_Data *data) {
    if (!handle || !data) return -1;
    float temp = 0.0f, hum = 0.0f;
    int ret = aht30_get((AHT30_Handle*)handle, &temp, &hum);
    if (ret == 0) {
        data->temperature = temp;
        data->humidity = hum;
    }
    return ret;
}
