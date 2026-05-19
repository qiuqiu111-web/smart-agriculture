# 智慧农业嵌入式Linux系统架构设计报告

---

## 一、语言与构建系统选型

### 1.1 C++17（推荐）

相比纯 C，C++ 在本项目中优势明显：

| 场景 | C | C++17 |
|------|---|-------|
| 线程管理 | `pthread_create` + 手动传参 | `std::thread` + lambda 捕获 |
| 线程安全队列 | 手写 ring buffer + mutex + cond | `std::queue<T>` + `std::mutex` + `std::condition_variable` |
| 资源管理 | 手动 close / free / destroy | RAII：析构函数自动释放 |
| 文件写入 | `fprintf` + 手动 `fflush` | `std::ofstream` + `std::endl` |
| 控制算法接口 | 函数指针 | `std::function` 或抽象基类虚函数 |
| 配置解析 | 手写 INI 解析 | JSON 库（nlohmann/json 单头文件） |
| 字符串处理 | `snprintf` / `strcat` | `std::string` / `std::ostringstream` / `fmt` |
| 已有 uart.c | 直接调用 | `extern "C"` 包裹头文件，零成本互调 |

**结论**：C++17，底层 uart.c 保留 C 不动，顶层全部用 C++ 搭建。

### 1.2 CMake（推荐）

```cmake
cmake_minimum_required(VERSION 3.10)
project(lubancat_agri LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 交叉编译时指定 toolchain:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
# 本地编译:
#   cmake -B build && cmake --build build
```

---

## 二、架构选型：单进程多线程

| 维度 | 单进程多线程 | 多进程 |
|------|-------------|--------|
| 内存开销 | 低（共享地址空间） | 高（独立地址空间） |
| 数据传递 | 指针/共享队列，零拷贝 | IPC（序列化开销） |
| 部署复杂度 | 单一可执行文件 | 多个可执行文件 |
| 崩溃隔离 | 一损俱损 | 子进程崩溃不传染 |
| 嵌入式适配 | 优 | 差 |

**选型理由**：LubanCat 资源有限，单进程零拷贝传递数据最高效。控制算法通过多态接口实现可替换，后续切换 ML 模型也无需拆进程。若未来确需隔离（如 Python 推理进程），可通过 Unix Socket 按需扩展。

---

## 三、协议帧格式（来自 STM32 侧定义）

### 3.1 传感器数据帧（STM32 → Linux），27 字节

```
┌──────────┬──────────┬────────────────────────┬──────────┬──────────┬──────────┐
│ HEADER_1 │ HEADER_2 │   Sensors_Data_Wire     │ CHECKSUM │ TAIL_1   │ TAIL_2   │
│   0xAA   │   0x55   │       (22 bytes)        │ (XOR)    │   0x55   │   0xAA   │
└──────────┴──────────┴────────────────────────┴──────────┴──────────┴──────────┘
  2 bytes              22 bytes packed struct     1 byte     2 bytes

Sensors_Data_Wire (__attribute__((packed)), 22 bytes):
┌─────────────┬──────────┬───────────────┬─────────────┬──────────┬──────────┬───────┬──────┬───────┬──────┐
│ temperature │ humidity │ soil_humidity │ illuminance │ seconds  │ minutes  │ hours │ date │ month │ year │
│   float(4)  │ float(4) │   float(4)    │  float(4)   │ uint8(1) │ uint8(1) │ u8(1) │ u8(1)│ u8(1) │ u8(1)│
└─────────────┴──────────┴───────────────┴─────────────┴──────────┴──────────┴───────┴──────┴───────┴──────┘
```

**帧总长**: `2 + 22 + 1 + 2 = 27` 字节  
**校验范围**: 帧头之后、校验字节之前的数据区（22 字节），算法为逐字节异或

### 3.2 命令帧（Linux → STM32），7 字节

```
┌──────────┬──────────┬────────────┬──────────┬──────────┬──────────┐
│ HEADER_1 │ HEADER_2 │ UART_Command│ CHECKSUM │ TAIL_1   │ TAIL_2   │
│   0xAA   │   0x55   │  (2 bytes)  │  (XOR)   │   0x55   │   0xAA   │
└──────────┴──────────┴────────────┴──────────┴──────────┴──────────┘

UART_Command (2 bytes):
┌────────────┬─────────┐
│ command_id │ payload │
│  uint8(1)  │ uint8(1)│
└────────────┴─────────┘
```

**帧总长**: `2 + 2 + 1 + 2 = 7` 字节

### 3.3 命令字约定（可扩展）

| command_id | 含义 | payload 取值范围 |
|------------|------|-------------------|
| 0x01 | 水泵控制 | 0=关, 1=开 |
| 0x02 | 电磁阀控制 | 0=关, 1=开 |
| 0x03 | 风扇控制 | 0=关, 1=开 |
| 0x04 | 补光灯控制 | 0=关, 1=开 |
| 0x05 | 加热器控制 | 0=关, 1=开 |

> 多个执行器同时动作时，连续发送多帧命令。每帧控制一个设备。

### 3.4 Linux 侧的协议职责

| 方向 | STM32 侧函数 | Linux 侧需要 |
|------|-------------|-------------|
| 传感器帧 | `frame_build_sensor()` — 打包发送 | **解析**传感器帧：找帧头 → 校验 → 提取 `Sensors_Data_Wire` |
| 命令帧 | `frame_parse_command()` — 接收解析 | **构建**命令帧：填 `command_id + payload` → 计算校验 → 加帧头帧尾 |

---

## 四、技术栈汇总

| 分类 | 技术 | 用途 |
|------|------|------|
| **语言** | C++17 | 主体框架（线程、队列、文件IO、协议） |
| **C 互操作** | `extern "C"` | 复用已有 uart.c |
| **构建** | CMake 3.10+ | 支持交叉编译 toolchain |
| **串口** | termios（已有 uart.c） | 底层 UART 读写 |
| **多线程** | `std::thread` | 并行处理收/发/处理/写盘 |
| **同步** | `std::mutex` + `std::condition_variable` | 线程安全队列 |
| **文件 IO** | `std::ofstream` | CSV 缓冲写入 |
| **时间** | `std::chrono` / `clock_gettime()` | 毫秒级时间戳 |
| **配置** | nlohmann/json（单头文件） | 解析 `config.json` |
| **日志** | 自研 `Logger` 类 | 带级别带时间戳的控制台/文件日志 |
| **后续 ML** | TensorFlow Lite C API 或 `popen` 调 Python | 模型推理（预留） |

**无额外依赖**：除 nlohmann/json（一个 .hpp 文件）外全部使用标准库。

---

## 五、任务拆分与线程职责

### 5.1 线程划分（4 个工作线程 + 主线程）

```
┌──────────────────────────────────────────────────────┐
│                    Main Thread                        │
│  加载配置 → 初始化各模块 → 启动线程 → 信号等待 → 清理  │
└──────────────────────────────────────────────────────┘
         │               │                │
         ▼               ▼                ▼
  ┌──────────┐   ┌──────────────┐   ┌──────────┐
  │ UART RX  │   │  Processing  │   │ UART TX  │
  │  Thread  │──▶│   Thread     │──▶│  Thread  │
  └──────────┘   └──────────────┘   └──────────┘
                        │
                        ▼
                 ┌──────────────┐
                 │  CSV Writer  │
                 │   Thread     │
                 └──────────────┘
```

### 5.2 任务详细说明

#### 任务 1：UartRxThread — 串口接收 + 帧同步

- **类**：`UartReceiver`
- **输入**：串口原始字节流
- **输出**：校验通过的完整传感器帧（`std::vector<uint8_t>`），推入 `rx_queue_`
- **实现要点**：
  - 内部维护一个 256 字节滑动缓冲区
  - 状态机扫描帧头 `0xAA 0x55`，命中后读取固定长度 27 字节
  - 校验尾部的 checksum（对数据区 22 字节异或）和帧尾 `0x55 0xAA`
  - 校验失败丢弃并递增错误计数；成功则 `emplace` 到队列
  - 阻塞读串口的数据量设置为 `VMIN=1, VTIME=0`（已有 uart.c 已配好）

#### 任务 2：ProcessingThread — 协议解析 + 控制决策

- **类**：`DataProcessor`
- **输入**：传感器帧（从 `rx_queue_` 取）
- **输出**：`CsvRow`（推入 `csv_queue_`），命令帧（推入 `tx_queue_`）
- **实现要点**：
  1. 从 `rx_queue_` pop 一帧
  2. 调用 `FrameCodec::parseSensorFrame()` 解析为 `SensorData` 结构体
  3. 打上当前时间戳
  4. 调用 `controller_->decide(sensor)` 获取 `ControlCmd`
  5. 将 `(timestamp, SensorData, ControlCmd)` 打包为 `CsvRow`，推入 `csv_queue_`
  6. 将 `ControlCmd` 转为若干帧命令（每个动作一帧），逐帧推入 `tx_queue_`
  7. 控制算法由 `std::unique_ptr<IController>` 持有，构造时注入

#### 任务 3：UartTxThread — 命令帧发送

- **类**：`UartTransmitter`
- **输入**：命令帧（从 `tx_queue_` 取）
- **输出**：串口发出的字节
- **实现要点**：
  - 阻塞等待 `tx_queue_` 有数据
  - 调用 `FrameCodec::buildCommandFrame()` 序列化
  - 通过 `uart_write()` 发送
  - 发送失败记录日志并继续（不重试，避免堆积）

#### 任务 4：CsvWriterThread — 数据持久化

- **类**：`CsvWriter`
- **输入**：`CsvRow`（从 `csv_queue_` 取）
- **输出**：`data/YYYY-MM-DD.csv` 文件
- **实现要点**：
  - 按日期自动切分：`data/2026-05-04.csv`, `data/2026-05-05.csv` ...
  - 内部缓存 `std::vector<CsvRow>`，攒满 N 行或超时 M 秒后批量 `flush`
  - 新建文件时自动写表头
  - **CSV 列**（每列含义）：
    ```
    timestamp_ms, temperature, humidity, soil_humidity, illuminance,
    year, month, date, hours, minutes, seconds,
    pump, valve, fan, light, heater
    ```
  - 前 11 列为传感器输入，后 5 列为控制输出——构成完整监督学习样本

#### 任务 5：IController — 控制算法（非独立线程，被 Processing 调用）

- **接口**：
  ```cpp
  class IController {
  public:
      virtual ~IController() = default;
      virtual ControlCmd decide(const SensorData& sensor) = 0;
  };
  ```
- **实现 1**：`ThresholdController` — 前期硬编码阈值规则
- **实现 2**：`MlController` — 后期加载 TFLite 模型或调用 Python 脚本
- 切换方式：`main.cpp` 中替换 `new ThresholdController(...)` 为 `new MlController(...)`

---

## 六、架构框架图

```
                      ┌─────────────────────────┐
                      │     config.json           │
                      │  uart设备、波特率、        │
                      │  控制阈值、csv目录等       │
                      └────────────┬────────────┘
                                   │ JsonConfig::load()
                                   ▼
┌──────────────────────────────────────────────────────────────────────┐
│                             main.cpp                                  │
│                                                                       │
│   1. JsonConfig cfg("config/config.json");                            │
│   2. Logger::init(cfg.log_level, cfg.log_file);                       │
│   3. CsvWriter csv(cfg.csv_dir, cfg.csv_flush_interval_ms);           │
│   4. auto controller = std::make_unique<ThresholdController>(cfg);    │
│   5. Pipeline pipeline(cfg, &csv, std::move(controller));             │
│   6. pipeline.start();          // 启动 4 个工作线程                   │
│   7. signal(SIGINT/SIGTERM);    // 等待退出信号                        │
│   8. pipeline.stop(); pipeline.join();                                │
│   9. csv.close();                                                     │
└──────────────────────────────────────────────────────────────────────┘

          ▼                          ▼                          ▼
┌────────────────────┐  ┌────────────────────────┐  ┌────────────────────┐
│   UartReceiver     │  │    DataProcessor        │  │  UartTransmitter   │
│   (rx thread)      │  │    (processing thread)  │  │  (tx thread)       │
│                    │  │                         │  │                    │
│ uart_read() 阻塞   │  │ rx_queue_.pop() 阻塞    │  │ tx_queue_.pop()    │
│        │           │  │        │                │  │ 阻塞               │
│        ▼           │  │        ▼                │  │        │           │
│ 扫描 0xAA 0x55     │  │ FrameCodec::            │  │        ▼           │
│ 收 27 字节         │  │   parseSensorFrame()    │  │ FrameCodec::       │
│ XOR 校验           │  │        │                │  │   buildCmdFrame()  │
│ 校验 0x55 0xAA     │  │        ▼                │  │        │           │
│        │           │  │ controller_->decide()   │  │        ▼           │
│        ▼           │  │        │                │  │ uart_write()       │
│ rx_queue_.push()   │  │   ┌────┴────┐           │  │                    │
│                    │  │   ▼         ▼           │  │                    │
│                    │  │ CsvRow   vector<frame>  │  │                    │
│                    │  │   │         │           │  │                    │
│                    │  │   ▼         ▼           │  │                    │
│                    │  │ csv_queue_  tx_queue_   │  │                    │
│                    │  │ .push()    .push()      │  │                    │
└────────────────────┘  └───────┬───────┬─────────┘  └────────────────────┘
                                │       │
                                ▼       └──────────────────────────┐
                     ┌──────────────────────┐                      │
                     │     CsvWriter         │                      │
                     │   (csv thread)        │                      │
                     │                       │                      │
                     │ csv_queue_.pop() 阻塞  │                      │
                     │        │              │                      │
                     │        ▼              │                      │
                     │ 缓冲区攒 N 行          │                      │
                     │ 或超时 M 秒           │                      │
                     │        │              │                      │
                     │        ▼              │                      │
                     │ std::ofstream         │                      │
                     │ data/2026-05-04.csv   │                      │
                     └──────────────────────┘                      │
                                                                   │
                     ┌─────────────────────────────────────────────┘
                     │
                     ▼
              STM32 ◀── UART ◀── 命令帧 (7字节/帧)


数据流（从左到右）：

  STM32 ──UART──▶ [RX: 收字节→拼帧→校验] ──frame──▶ [RX队列]
                                                           │
                                                           ▼
                                                    [Processing]
                                                  解析帧/控制决策
                                                           │
                                          ┌────────────────┼────────────────┐
                                          ▼                                 ▼
                                     [CSV队列]                          [TX队列]
                                          │                                 │
                                          ▼                                 ▼
                                   [CSV Writer]                      [TX: 组帧→发送]
                                          │                                 │
                                          ▼                                 ▼
                                   data/*.csv                   UART ──▶ STM32
```

---

## 七、C++ 类设计概览

### 7.1 ThreadSafeQueue\<T\>

```cpp
// include/core/thread_safe_queue.hpp
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item);         // 生产者：放入元素，notify_one
    T    pop();                // 消费者：阻塞等待直到有元素
    bool tryPop(T& item);      // 非阻塞获取
    void stop();               // 唤醒所有等待者，用于关闭
    bool isStopped() const;
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};
```

### 7.2 FrameCodec（纯静态方法）

```cpp
// include/protocol/frame_codec.hpp
struct FrameCodec {
    // 解析传感器帧 (27字节 → SensorData)
    static std::optional<SensorData>
        parseSensorFrame(const uint8_t* buf, size_t len);

    // 构建命令帧 (cmd_id + payload → 7字节)
    static std::vector<uint8_t>
        buildCommandFrame(uint8_t cmd_id, uint8_t payload);

    // 校验和计算
    static uint8_t checksum(const uint8_t* data, size_t len);
};
```

### 7.3 IController 接口 + 实现

```cpp
// include/control/controller.hpp
class IController {
public:
    virtual ~IController() = default;
    virtual ControlCmd decide(const SensorData& sensor) = 0;
};

// src/control/threshold_controller.cpp
class ThresholdController : public IController {
public:
    explicit ThresholdController(const ControlThresholds& cfg);
    ControlCmd decide(const SensorData& sensor) override;
private:
    ControlThresholds thresholds_;
};

// src/control/ml_controller.cpp (预留)
class MlController : public IController {
    // 后续实现: TFLite 推理 或 popen("python3 infer.py")
};
```

### 7.4 CsvWriter

```cpp
// include/datalog/csv_writer.hpp
class CsvWriter {
public:
    CsvWriter(const std::string& dataDir, int flushIntervalMs);
    void writeRow(const CsvRow& row);
    void flush();
    void close();
private:
    void checkRotate();              // 跨天自动切文件
    void ensureHeader();             // 新建文件写入表头
    std::string currentFile_;        // 当前 csv 路径
    std::ofstream ofs_;
    std::vector<CsvRow> buffer_;
    // ...
};
```

### 7.5 Pipeline（总装类）

```cpp
// include/core/pipeline.hpp
class Pipeline {
public:
    Pipeline(const JsonConfig& cfg, CsvWriter* csv,
             std::unique_ptr<IController> controller);
    void start();                    // 创建并启动 4 个线程
    void stop();                     // 设置停止标志，唤醒所有队列
    void join();                     // 等待所有线程退出
private:
    void rxLoop();                   // UART 接收线程主循环
    void processLoop();              // 数据处理线程主循环
    void txLoop();                   // UART 发送线程主循环
    void csvLoop();                  // CSV 写入线程主循环

    ThreadSafeQueue<std::vector<uint8_t>> rxQueue_;
    ThreadSafeQueue<CsvRow>               csvQueue_;
    ThreadSafeQueue<std::vector<uint8_t>> txQueue_;

    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    CsvWriter* csv_;
    std::unique_ptr<IController> controller_;
    int uartFd_;
};
```

---

## 八、目录结构

```
lubancat/
│
├── uart/                              # [已有] C 语言 UART 驱动（保留不动）
│   ├── uart.h                         #   导出 C 接口
│   └── uart.c                         #   termios 实现
│
├── include/                           # C++ 头文件
│   ├── protocol/
│   │   └── frame_codec.hpp            #   帧解析/构建 + 结构体定义
│   ├── control/
│   │   └── controller.hpp             #   IController 抽象接口 + ControlCmd/SensorData
│   ├── datalog/
│   │   └── csv_writer.hpp             #   CsvWriter 类
│   ├── core/
│   │   ├── pipeline.hpp               #   Pipeline 总装类
│   │   └── thread_safe_queue.hpp      #   ThreadSafeQueue<T> 模板
│   └── utils/
│       ├── logger.hpp                 #   Logger 单例
│       └── json_config.hpp            #   JsonConfig 配置加载
│
├── src/                               # C++ 源文件
│   ├── protocol/
│   │   └── frame_codec.cpp            #   帧解析/构建实现
│   ├── control/
│   │   ├── threshold_controller.cpp   #   硬编码阈值规则
│   │   └── ml_controller.cpp          #   ML 模型推理（预留）
│   ├── datalog/
│   │   └── csv_writer.cpp             #   CSV 缓冲写/日切实现
│   ├── core/
│   │   └── pipeline.cpp               #   线程创建/主循环/销毁
│   └── utils/
│       ├── logger.cpp                 #   日志实现
│       └── json_config.cpp            #   配置加载实现
│
├── third_party/                       # 第三方库（可选）
│   └── nlohmann/
│       └── json.hpp                   #   单头文件 JSON 库
│
├── config/
│   └── config.json                    #   运行时配置文件
│
├── data/                              # 运行时 CSV 输出（自动创建）
│   └── .gitkeep
│
├── cmake/
│   └── aarch64-linux-gnu.cmake        #   交叉编译 toolchain
│
├── main.cpp                           #   入口
├── CMakeLists.txt                     #   构建
└── docs/
    └── architecture_report.md         #   本报告
```

---

## 九、配置文件示例

```json
{
    "uart": {
        "device": "/dev/ttyS3",
        "baudrate": 115200,
        "databits": 8,
        "parity": "none",
        "stopbits": 1
    },
    "control": {
        "algorithm": "threshold",
        "thresholds": {
            "soil_humidity_min": 30.0,
            "temperature_max": 35.0,
            "illuminance_min": 5000.0
        }
    },
    "csv": {
        "output_dir": "data",
        "flush_interval_ms": 5000,
        "buffer_max_rows": 100
    },
    "log": {
        "level": "info",
        "file": "logs/app.log"
    }
}
```

---

## 十、CMakeLists.txt 骨架

```cmake
cmake_minimum_required(VERSION 3.10)
project(lubancat_agri VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- 源文件 ----------
set(C_SOURCES
    uart/uart.c
)

set(CXX_SOURCES
    main.cpp
    src/protocol/frame_codec.cpp
    src/control/threshold_controller.cpp
    src/control/ml_controller.cpp
    src/datalog/csv_writer.cpp
    src/core/pipeline.cpp
    src/utils/logger.cpp
    src/utils/json_config.cpp
)

# ---------- 可执行文件 ----------
add_executable(${PROJECT_NAME}
    ${C_SOURCES}
    ${CXX_SOURCES}
)

# ---------- 头文件路径 ----------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/uart
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann
)

# ---------- 链接 ----------
target_link_libraries(${PROJECT_NAME} PRIVATE
    pthread        # std::thread 底层依赖
)

# ---------- 安装 ----------
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/lubancat_agri)
```

**交叉编译命令**：
```bash
# 本机编译（直接在 LubanCat 上）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# 交叉编译（在 PC 上给 LubanCat 编译）
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

---

## 十一、实现流程与步骤

### Phase 1：基础设施（1 天）

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1.1 | 创建目录结构 + CMakeLists.txt | 项目骨架可编译 |
| 1.2 | 实现 `ThreadSafeQueue<T>` 模板 | `include/core/thread_safe_queue.hpp` |
| 1.3 | 实现 `Logger` 类 | `utils/logger.hpp`, `logger.cpp` |
| 1.4 | 实现 `JsonConfig` 加载 | `utils/json_config.hpp`, `json_config.cpp` + `config.json` |

### Phase 2：协议层（1 天）

| 步骤 | 内容 | 产出 |
|------|------|------|
| 2.1 | 定义 C++ 侧 `SensorData` / `ControlCmd` 结构体 | `include/protocol/frame_codec.hpp` |
| 2.2 | 实现 `parseSensorFrame()` — 帧头扫描 + checksum 校验 + 提取数据 | `src/protocol/frame_codec.cpp` |
| 2.3 | 实现 `buildCommandFrame()` — 填 cmd_id/payload → 计算校验 → 加帧头尾 | 同上 |
| 2.4 | 编写单元测试（用已知帧验证解析/构建正确性） | `tests/test_frame_codec.cpp` |

### Phase 3：控制算法 + CSV 写入（1 天）

| 步骤 | 内容 | 产出 |
|------|------|------|
| 3.1 | 定义 `IController` 接口 | `include/control/controller.hpp` |
| 3.2 | 实现 `ThresholdController` | `src/control/threshold_controller.cpp` |
| 3.3 | 实现 `CsvWriter`（缓冲写 + 日期切分 + 自动表头） | `datalog/csv_writer.hpp`, `csv_writer.cpp` |

### Phase 4：管道集成（2 天）

| 步骤 | 内容 | 产出 |
|------|------|------|
| 4.1 | 实现 `Pipeline` 构造函数（初始化队列、打开串口、注入 controller） | `core/pipeline.cpp` |
| 4.2 | 实现 `rxLoop()` — UART 接收 + 帧同步 + 校验 | 同上 |
| 4.3 | 实现 `processLoop()` — 解析 → 决策 → 写 csv + 构建命令 | 同上 |
| 4.4 | 实现 `txLoop()` — 串口发送命令帧 | 同上 |
| 4.5 | 实现 `csvLoop()` — 缓冲写盘 | 同上 |
| 4.6 | 实现 `start/stop/join` — 线程生命周期管理 | 同上 |

### Phase 5：主入口 + 集成测试（1 天）

| 步骤 | 内容 | 产出 |
|------|------|------|
| 5.1 | 实现 `main.cpp`（加载配置 → 启动管道 → 信号等待 → 清理） | `main.cpp` |
| 5.2 | 编写 STM32 模拟器（发送模拟传感器帧，接收命令帧打印） | `tests/stm32_simulator.cpp` |
| 5.3 | 端到端测试（模拟器 ↔ Linux 程序通过虚拟串口通信） | — |

### Phase 6：交叉编译与部署（0.5 天）

| 步骤 | 内容 |
|------|------|
| 6.1 | 配置 `aarch64-linux-gnu` 交叉编译 toolchain |
| 6.2 | 编译 → scp 到 LubanCat → 连接 STM32 实机验证 |

---

## 十二、优雅关闭流程

```
  SIGINT/SIGTERM
        │
        ▼
  signal_handler() 设置 running_ = false
        │
        ├──▶ rxQueue_.stop()    → 唤醒阻塞在 pop() 的 RX 线程
        ├──▶ csvQueue_.stop()   → 唤醒阻塞在 pop() 的 CSV 线程
        ├──▶ txQueue_.stop()    → 唤醒阻塞在 pop() 的 TX 线程
        │
        ▼
  pipeline.join()
        │
        ├──▶ rx thread     退出（close uart fd）
        ├──▶ processing    退出（处理完队列中剩余数据）
        ├──▶ tx thread     退出
        └──▶ csv thread    退出（flush 缓冲区中剩余行）
        │
        ▼
  csv.close()    → 最终 fflush + 关闭文件
  uart_close()
  Logger::shutdown()
```

---

## 十三、后续扩展方向

| 扩展 | 方案 |
|------|------|
| 控制算法 → ML 模型 | 实现 `MlController`，内部加载 TFLite 模型或 `popen("python3 infer.py")` |
| 远程监控 | 新增 `MqttReporter` 线程，定时从共享状态读取最新数据上报云端 |
| 参数在线调整 | 新增 TCP 监听线程，接收 JSON 命令修改控制阈值（`std::atomic` 更新） |
| 数据回放 | 新增 `replay` 模式：读取 CSV 历史数据 → 逐行推入 `rx_queue_` → 验证算法效果 |
| 掉电保护 | CSV 改为每次 `flush` 后 `fsync`；或使用 SQLite 替代 CSV |

---

## 十四、总结

| 项目 | 选择 |
|------|------|
| **语言** | C++17（底层 uart.c 保持 C，`extern "C"` 调用） |
| **构建** | CMake（支持交叉编译 toolchain） |
| **架构** | 单进程 4 线程 |
| **队列** | 3 个 `ThreadSafeQueue`（生产者-消费者模式，mutex + cond） |
| **控制算法** | `IController` 多态接口，前期 `ThresholdController`，后期 `MlController` |
| **协议** | 已适配 STM32 帧格式：传感器帧 27 字节，命令帧 7 字节 |
| **数据闭环** | CSV 每行 = 传感器输入 + 控制输出，即 (X, y) 监督学习样本对 |

**四个线程的输入输出关系**：

```
UartRx ──frame──▶ [rxQueue] ──frame──▶ Processing ──┬──▶ [csvQueue] ──row──▶ CsvWriter
                                                     │
                                                     └──▶ [txQueue]  ──frame──▶ UartTx
```
