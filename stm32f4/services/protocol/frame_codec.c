#include "frame_codec.h"
#include <string.h>

// ============ 校验和 ============
uint8_t frame_checksum(uint8_t *data, uint8_t len) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// ============ 传感器数据 → 线格式 ============
static void _pack_sensor_data(Sensors_Data *data, Sensors_Data_Wire *wire) {
    wire->temperature      = data->temperature;
    wire->humidity         = data->humidity;
    wire->soil_temperature = data->soil_temperature;
    wire->soil_humidity    = data->soil_humidity;
    wire->illuminance      = data->illuminance;
}

// ============ 构建传感器帧 ============
void frame_build_sensor(Sensors_Data *data, uint8_t *out_buf) {
    Sensors_Data_Wire wire;
    uint8_t index = 0;

    _pack_sensor_data(data, &wire);

    out_buf[index++] = UART_HEADER_1;
    out_buf[index++] = UART_HEADER_2;

    memcpy(&out_buf[index], &wire, UART_DATA_SIZE);
    index += UART_DATA_SIZE;

    out_buf[index++] = frame_checksum(&out_buf[2], UART_DATA_SIZE);

    out_buf[index++] = UART_TAIL_1;
    out_buf[index++] = UART_TAIL_2;
}

// ============ 解析命令帧 ============
int frame_parse_command(uint8_t *buf, uint16_t buf_len, UART_Command *out_cmd) {
    if (!buf || !out_cmd) return -1;

    for (uint16_t i = 0; i + CMD_FRAME_SIZE <= buf_len; i++) {
        if (buf[i] != UART_HEADER_1 || buf[i + 1] != UART_HEADER_2) continue;

        uint8_t *pdata   = &buf[i + 2];
        uint8_t  checksum = buf[i + 2 + CMD_DATA_SIZE];
        uint8_t  tail1    = buf[i + 2 + CMD_DATA_SIZE + 1];
        uint8_t  tail2    = buf[i + 2 + CMD_DATA_SIZE + 2];

        if (frame_checksum(pdata, CMD_DATA_SIZE) != checksum) continue;
        if (tail1 != UART_TAIL_1 || tail2 != UART_TAIL_2) continue;

        out_cmd->command_id = pdata[0];
        out_cmd->payload    = pdata[1];
        return 0;
    }
    return -1;
}
