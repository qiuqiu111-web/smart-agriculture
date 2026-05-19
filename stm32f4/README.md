# 智慧农业传感器演示系统

基于 STM32F429 + FreeRTOS 的农业嵌入式系统。四层架构设计，支持多传感器采集、UART 通信协议、远程水泵控制。

## 系统架构

```
┌──────────────────────────────────────────┐
│  app/        应用层                       │
│  任务调度、数据队列、业务编排               │
├──────────────────────────────────────────┤
│  services/   服务层                       │
│  传感器管理、通信协议、水泵驱动、时间服务    │
├──────────────────────────────────────────┤
│  Core/       HAL 抽象层                   │
│  UART / I2C / ADC / TIM / DMA / GPIO     │
├──────────────────────────────────────────┤
│  Drivers/    BSP 层                       │
│  CMSIS + STM32F4xx HAL Driver            │
├──────────────────────────────────────────┤
│  Middlewares/  OS 层                      │
│  FreeRTOS + CMSIS-RTOS v1                │
└──────────────────────────────────────────┘
```

## 传感器列表

| 传感器 | 通信协议 | 功能 |
|--------|----------|------|
| DHT22 | 单总线 (GPIO) | 空气温湿度 |
| DS18B20 | 1-Wire (GPIO) | 土壤温度 |
| DS3231 | I2C | 实时时钟 RTC |
| GY30 (BH1750) | I2C | 光照强度 |
| 土壤湿度 (LM393) | ADC | 土壤湿度 |

全部驱动采用**非阻塞状态机**设计，通过 `Sensors_Ops` 虚函数表统一接口。

## FreeRTOS 任务设计

| 任务 | 优先级 | 职责 |
|------|--------|------|
| ReceiveCommandTask | osPriorityHigh (5) | IDLE 中断通知 → 帧解析 → 命令入队 |
| ControlCommandTask | osPriorityAboveNormal (4) | 命令出队 → 执行控制 (水泵调速) |
| GetSensorsDataTask | osPriorityNormal (3) | 每 2s 采集传感器 → 数据入队 |
| SendDataTask | osPriorityBelowNormal (2) | 数据出队 → UART 发送到上位机 |

数据流：`传感器 → sensorsDataQueue → UART TX`  
控制流：`UART RX → IDLE中断 → Task Notification → commandQueue → 水泵PWM`

## 项目结构

```
├── app/                             # 应用层
│   ├── app_config.h                 # 集中配置 (优先级/栈/队列/采样周期)
│   ├── queues.h / queues.c          # FreeRTOS 队列定义
│   ├── task_sensor.c                # 传感器采集任务
│   ├── task_send.c                  # 数据发送任务
│   ├── task_recv.c                  # 命令接收任务
│   └── task_control.c               # 命令执行任务
├── services/                        # 服务层
│   ├── protocol/
│   │   ├── frame_codec.h            # 协议帧定义 (常量/结构体/API)
│   │   └── frame_codec.c            # 纯协议逻辑 (打包/解析/校验)
│   ├── uart_adapter/
│   │   ├── uart_hal.h               # UART HAL 适配层 API
│   │   └── uart_hal.c               # DMA 管理 / IDLE 中断 / 发送
│   ├── sensors/
│   │   ├── manager.h / manager.c    # 传感器管理器
│   │   ├── adapters.h / adapters.c  # 传感器适配器 (统一 get_data)
│   │   └── devices/                 # 传感器驱动
│   │       ├── func_config.h        # 硬件配置宏封装
│   │       ├── DHT22.h/c
│   │       ├── DS18B20.h/c
│   │       ├── DS3231.h/c
│   │       ├── GY30.h/c
│   │       └── SoilHumidity.h/c
│   ├── pump/
│   │   └── pump_driver.h/c          # 水泵 PWM 驱动 (VTable 模式)
│   └── time/
│       └── time_service.h/c         # DWT 时间戳 / 微秒延时
├── Core/                            # STM32CubeMX 生成
│   ├── Inc/                         # 外设头文件
│   └── Src/                         # 外设初始化 + 中断处理
├── Drivers/                         # CMSIS + STM32F4xx HAL
├── Middlewares/                     # FreeRTOS + CMSIS-RTOS
├── CMakeLists.txt
└── sensor_demo.ioc                  # CubeMX 项目配置
```

## 快速开始

1. 使用 STM32CubeMX 打开 `sensor_demo.ioc` 生成初始化代码
2. 根据硬件连接修改 `services/sensors/devices/func_config.h` 中的 GPIO/I2C/ADC 句柄
3. 编译：

```bash
mkdir -p build && cd build
cmake ..
make
```

4. 烧录到 STM32F429 开发板
5. 上位机通过 USART6 (115200 8N1) 发送命令 `AA 55 01 <speed> <checksum> 55 AA` 控制水泵

## 硬件平台

- MCU: STM32F429
- RTOS: FreeRTOS (CMSIS-RTOS v1 封装)
- 工具链: ARM GCC (gcc-arm-none-eabi)
- 构建系统: CMake
