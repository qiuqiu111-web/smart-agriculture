#include "uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* 内部辅助函数 */
static speed_t _baudrate_to_speed(int baudrate)
{
    switch (baudrate) {
        case 9600:     return B9600;
        case 19200:    return B19200;
        case 38400:    return B38400;
        case 57600:    return B57600;
        case 115200:   return B115200;
        case 230400:   return B230400;
        default:       return B9600; /* 默认 */
    }
}

static void _set_error_msg(char *error_msg, const char *prefix, int errnum)
{
    if (error_msg) {
        snprintf(error_msg, 256, "%s: %s", prefix, strerror(errnum));
    }
}

static void _print_error(const char *prefix, int errnum)
{
    if(prefix) {
        fprintf(stderr, "%s: %s\n", prefix, strerror(errnum));
    }
}

static void _set_custom_error_msg(char *error_msg, const char *msg)
{
    if (error_msg) {
        snprintf(error_msg, 256, "%s", msg);
    }
}

int uart_open(const char *path, int *fd)
{
    if (!path || !fd) {
        return UART_ERR_INVALID_PARAM;
    }

    /* 可读可写，非控制，非阻塞打开 */
    int f = open(path, O_RDWR | O_NOCTTY | O_NDELAY); 
    if (f < 0) {
        return UART_ERR_OPEN_FAILED;
    }

    /* 恢复阻塞模式 */
    int flags = fcntl(f, F_GETFL, 0);
    if (flags < 0) {
        close(f);
        return UART_ERR_OPEN_FAILED;
    }
    flags &= ~O_NDELAY;
    if (fcntl(f, F_SETFL, flags) < 0) {
        close(f);
        return UART_ERR_OPEN_FAILED;
    }

    /* 检查是否为终端设备，防止为普通文件 */
    if (!isatty(f)) {
        close(f);
        return UART_ERR_OPEN_FAILED;
    }

    *fd = f;
    return UART_SUCCESS;
}

int uart_config(int fd, int baudrate, int databits, int parity, int stopbits)
{
    struct termios tio;

    /* 获取当前配置 */
    if (tcgetattr(fd, &tio) < 0) {
        return UART_ERR_CONFIG_FAILED;
    }

    /* 清空缓冲区 */
    tcflush(fd, TCIOFLUSH);

    /* 设置输入输出波特率 */
    speed_t speed = _baudrate_to_speed(baudrate);
    cfsetspeed(&tio, speed);

    /* 设置数据位 */
    tio.c_cflag &= ~CSIZE;
    switch (databits) {
        case 5:
            tio.c_cflag |= CS5;
            break;
        case 6:
            tio.c_cflag |= CS6;
            break;
        case 7:
            tio.c_cflag |= CS7;
            break;
        case 8:
            tio.c_cflag |= CS8;
            break;
        default:
            return UART_ERR_INVALID_PARAM;
    }

    /* 设置校验位 */
    switch (parity) {
        case UART_PARITY_NONE:
            tio.c_cflag &= ~PARENB;
            tio.c_iflag &= ~INPCK;
            break;
        case UART_PARITY_ODD:
            tio.c_cflag |= PARENB;
            tio.c_cflag |= PARODD;
            tio.c_iflag |= INPCK;
            break;
        case UART_PARITY_EVEN:
            tio.c_cflag |= PARENB;
            tio.c_cflag &= ~PARODD;
            tio.c_iflag |= INPCK;
            break;
        default:
            return UART_ERR_INVALID_PARAM;
    }

    /* 设置停止位 */
    switch (stopbits) {
        case 1:
            tio.c_cflag &= ~CSTOPB;
            break;
        case 2:
            tio.c_cflag |= CSTOPB;
            break;
        default:
            return UART_ERR_INVALID_PARAM;
    }

    /* 其他设置：（防止数据变为控制字符）*/
    tio.c_cflag |= (CLOCAL | CREAD); /* 设置本地连接和读取使能 */
    tio.c_cflag &= ~CRTSCTS; /* 禁用硬件流控制 */
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* 禁用规范模式、回显、回显错误、信号 */
    tio.c_oflag &= ~OPOST; /* 禁用输出处理 */
    tio.c_iflag &= ~(IXON | IXOFF | IXANY); /* 禁用软件流控制 */

    /* 设置阻塞模式：等待至少1个字符，无限期等待 */
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    /* 应用配置 */
    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        return UART_ERR_CONFIG_FAILED;
    }

    return UART_SUCCESS;
}

int uart_read(int fd, void *buf, size_t count, size_t *bytes_read)
{
    if (!buf || !bytes_read) {
        return UART_ERR_INVALID_PARAM;
    }

    ssize_t n = read(fd, buf, count);
    if (n < 0) {
        return UART_ERR_READ_FAILED;
    }

    *bytes_read = (size_t)n;
    return UART_SUCCESS;
}

int uart_write(int fd, const void *buf, size_t count, size_t *bytes_written)
{
    if (!buf || !bytes_written) {
        return UART_ERR_INVALID_PARAM;
    }

    ssize_t n = write(fd, buf, count);
    if (n < 0) {
        return UART_ERR_WRITE_FAILED;
    }

    *bytes_written = (size_t)n;
    return UART_SUCCESS;
}

int uart_close(int fd)
{
    if (close(fd) < 0) {
        return UART_ERR_CLOSE_FAILED;
    }
    return UART_SUCCESS;
}

const char *uart_strerror(int errcode)
{
    switch (errcode) {
        case UART_SUCCESS:
            return "Success";
        case UART_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case UART_ERR_OPEN_FAILED:
            return "Open device failed";
        case UART_ERR_CONFIG_FAILED:
            return "Configure device failed";
        case UART_ERR_READ_FAILED:
            return "Read data failed";
        case UART_ERR_WRITE_FAILED:
            return "Write data failed";
        case UART_ERR_CLOSE_FAILED:
            return "Close device failed";
        default:
            return "Unknown error";
    }
}