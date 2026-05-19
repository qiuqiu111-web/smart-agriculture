#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart.h"

int main(int argc, char *argv[])
{
    int fd;
    int err;
    char error_msg[256];
    const char *path = (argc > 1) ? argv[1] : "/dev/ttyS3";

    printf("UART API Example\n");
    printf("Opening device: %s\n", path);

    // 打开串口
    err = uart_open(path, &fd, error_msg);
    if (err != UART_SUCCESS) {
        printf("Failed to open: %s\n", error_msg);
        return EXIT_FAILURE;
    }

    printf("Device opened successfully (fd=%d)\n", fd);

    // 配置串口：115200 8N1
    printf("Configuring UART: 115200 8N1\n");
    err = uart_config(fd, UART_BAUD_115200, UART_DATA_BITS_8,
                      UART_PARITY_NONE, UART_STOP_BITS_1, error_msg);
    if (err != UART_SUCCESS) {
        printf("Failed to config: %s\n", error_msg);
        uart_close(fd, NULL);
        return EXIT_FAILURE;
    }

    printf("Configuration applied\n");

    // 发送数据
    const char *send_data = "Hello UART!\n";
    size_t written;
    printf("Sending data: %s", send_data);
    err = uart_write(fd, send_data, strlen(send_data), &written, error_msg);
    if (err != UART_SUCCESS) {
        printf("Failed to write: %s\n", error_msg);
    } else {
        printf("Written %zu bytes\n", written);
    }

    // 接收数据（等待1秒）
    printf("Waiting for response (timeout 1 second)...\n");
    char recv_buf[1024];
    size_t read;
    err = uart_read(fd, recv_buf, sizeof(recv_buf)-1, &read, error_msg);
    if (err == UART_SUCCESS) {
        if (read > 0) {
            recv_buf[read] = '\0';
            printf("Received %zu bytes: %s\n", read, recv_buf);
        } else {
            printf("No data received (timeout)\n");
        }
    } else {
        printf("Read failed: %s\n", error_msg);
    }

    // 关闭串口
    printf("Closing device\n");
    err = uart_close(fd, error_msg);
    if (err != UART_SUCCESS) {
        printf("Failed to close: %s\n", error_msg);
        return EXIT_FAILURE;
    }

    printf("Example completed successfully\n");
    return EXIT_SUCCESS;
}