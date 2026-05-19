#ifndef FRAME_CODEC_H
#define FRAME_CODEC_H

#include <stdint.h>
#include "manager.h"

// 帧头和帧尾
#define UART_HEADER_1  0xAA
#define UART_HEADER_2  0x55
#define UART_TAIL_1    0x55
#define UART_TAIL_2    0xAA

// ---------- 传感器数据帧 ----------
#pragma pack(push, 1)
typedef struct {
    float temperature;
    float humidity;
    float soil_temperature;
    float soil_humidity;
    float illuminance;
} Sensors_Data_Wire;
#pragma pack(pop)

#define UART_DATA_SIZE  sizeof(Sensors_Data_Wire)
#define UART_FRAME_SIZE (UART_DATA_SIZE + 5) // 帧头2 + 数据 + 校验1 + 帧尾2

// ---------- 命令帧 ----------
typedef struct {
    uint8_t command_id;
    uint8_t payload;
} UART_Command;

#define CMD_DATA_SIZE   sizeof(UART_Command)
#define CMD_FRAME_SIZE  (CMD_DATA_SIZE + 5)

// ---------- 纯协议函数(与硬件无关, 可单元测试) ----------
uint8_t frame_checksum(uint8_t *data, uint8_t len);
void    frame_build_sensor(Sensors_Data *data, uint8_t *out_buf);
int     frame_parse_command(uint8_t *buf, uint16_t buf_len, UART_Command *out_cmd);

#endif // FRAME_CODEC_H
