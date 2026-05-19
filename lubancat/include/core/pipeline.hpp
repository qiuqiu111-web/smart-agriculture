#pragma once

#include "core/thread_safe_queue.hpp"
#include "control/controller.hpp"
#include "datalog/csv_writer.hpp"
#include "protocol/frame_codec.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// 数据处理管道：串口 RX -> 处理决策 -> 串口 TX + CSV 写盘
class Pipeline {
public:
    Pipeline(int uartFd, CsvWriter* csv, std::unique_ptr<IController> controller,
             int sensorBufMs = 60000);

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // 启动全部工作线程
    void start();

    // 通知所有线程停止（非阻塞）
    void stop();

    // 等待所有线程退出（阻塞）
    void join();

private:
    void rxLoop();       // 串口接收 + 帧同步
    void processLoop();  // 协议解析 + 控制决策
    void txLoop();       // 串口发送
    void csvLoop();      // CSV 缓冲写盘

    int uartFd_;
    CsvWriter* csv_;
    std::unique_ptr<IController> controller_;

    // 传感器数据缓冲（1分钟均值）
    int sensorBufMs_;
    SensorData sensorSum_{};
    int sensorCount_ = 0;
    std::chrono::steady_clock::time_point sensorBufStart_;

    // 线程间通信队列
    ThreadSafeQueue<std::vector<uint8_t>> rxQueue_;
    ThreadSafeQueue<CsvRow>              csvQueue_;
    ThreadSafeQueue<std::vector<uint8_t>> txQueue_;

    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};  // 用std::atomic使数据读写原子化
};
