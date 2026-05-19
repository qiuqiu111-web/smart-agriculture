#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

/* 错误码定义 */
#define UART_SUCCESS             0
#define UART_ERR_INVALID_PARAM  -1
#define UART_ERR_OPEN_FAILED    -2
#define UART_ERR_CONFIG_FAILED  -3
#define UART_ERR_READ_FAILED    -4
#define UART_ERR_WRITE_FAILED   -5
#define UART_ERR_CLOSE_FAILED   -6

/* 波特率配置宏 */
#define UART_BAUD_9600     9600
#define UART_BAUD_19200    19200
#define UART_BAUD_38400    38400
#define UART_BAUD_57600    57600
#define UART_BAUD_115200   115200
#define UART_BAUD_230400   230400

/* 数据位配置宏 */
#define UART_DATA_BITS_5   5
#define UART_DATA_BITS_6   6
#define UART_DATA_BITS_7   7
#define UART_DATA_BITS_8   8

/* 校验位配置宏 */
#define UART_PARITY_NONE   0
#define UART_PARITY_ODD    1
#define UART_PARITY_EVEN   2

/* 停止位配置宏 */
#define UART_STOP_BITS_1   1
#define UART_STOP_BITS_2   2

/* 函数原型 */

/**
 * 打开串口设备
 * @param path 设备路径，如 "/dev/ttyS3"
 * @param fd 输出参数，成功时返回文件描述符
 * @return 错误码（UART_SUCCESS 表示成功）
 */
int uart_open(const char *path, int *fd);

/**
 * 配置串口参数
 * @param fd 文件描述符
 * @param baudrate 波特率（使用 UART_BAUD_* 宏）
 * @param databits 数据位（使用 UART_DATA_BITS_* 宏）
 * @param parity 校验位（使用 UART_PARITY_* 宏）
 * @param stopbits 停止位（使用 UART_STOP_BITS_* 宏）
 * @return 错误码
 */
int uart_config(int fd, int baudrate, int databits, int parity, int stopbits);

/**
 * 读取串口数据
 * @param fd 文件描述符
 * @param buf 接收缓冲区
 * @param count 期望读取的字节数
 * @param bytes_read 实际读取的字节数（输出参数）
 * @return 错误码
 */
int uart_read(int fd, void *buf, size_t count, size_t *bytes_read);

/**
 * 写入串口数据
 * @param fd 文件描述符
 * @param buf 发送缓冲区
 * @param count 要写入的字节数
 * @param bytes_written 实际写入的字节数（输出参数）
 * @return 错误码
 */
int uart_write(int fd, const void *buf, size_t count, size_t *bytes_written);

/**
 * 关闭串口
 * @param fd 文件描述符
 * @return 错误码
 */
int uart_close(int fd);

/**
 * 获取串口错误码对应的描述字符串
 * @param errcode 错误码
 * @return 错误描述字符串（静态常量，无需释放）
 */
const char *uart_strerror(int errcode);


#endif 