#include "core/pipeline.hpp"
#include "control/controller.hpp"
#include "datalog/csv_writer.hpp"
#include "utils/json_config.hpp"
#include "utils/logger.hpp"

extern "C" {
#include "uart.h"
}

#include <csignal>
#include <atomic>
#include <string>
#include <thread>

static int parseParity(const std::string& s) {
    if (s == "odd")  return UART_PARITY_ODD;
    if (s == "even") return UART_PARITY_EVEN;
    return UART_PARITY_NONE;
}

static std::atomic<bool> gRunning{true};

static void signalHandler(int /*sig*/) {
    gRunning = false;
}

int main(int argc, char* argv[]) {
    // ---- 加载配置 ----
    const char* configPath = (argc > 1) ? argv[1] : "config/config.json";
    JsonConfig cfg;
    if (!cfg.load(configPath)) {
        std::fprintf(stderr, "无法加载配置文件: %s\n", configPath);
        return 1;
    }

    // ---- 初始化日志 ----
    Logger::instance().init(
        logLevelFromString(cfg.getString("log.level", "info")),
        cfg.getString("log.file", "")
    );
    LOG_INFO("智慧农业系统启动中...");

    // ---- 打开串口 ----
    std::string device = cfg.getString("uart.device", "/dev/ttyS3");
    int uartFd = -1;
    int ret = uart_open(device.c_str(), &uartFd);
    if (ret != UART_SUCCESS) {
        LOG_ERROR("无法打开 %s: %s", device.c_str(), uart_strerror(ret));
        return 1;
    }
    LOG_INFO("串口 %s 已打开 (fd=%d)", device.c_str(), uartFd);

    ret = uart_config(uartFd,
        cfg.getInt("uart.baudrate", 115200),
        cfg.getInt("uart.databits", 8),
        parseParity(cfg.getString("uart.parity", "none")),
        cfg.getInt("uart.stopbits", 1));
    if (ret != UART_SUCCESS) {
        LOG_ERROR("串口配置失败: %s", uart_strerror(ret));
        uart_close(uartFd);
        return 1;
    }
    LOG_INFO("串口已配置");

    // ---- 创建控制器 ----
    std::string algo = cfg.getString("control.algorithm", "expert_rule");
    std::unique_ptr<IController> controller;

    if (algo == "expert_rule") {
        ExpertRuleConfig ruleCfg;
        ruleCfg.soilTargetMidline = static_cast<float>(
            cfg.getDouble("control.soil.target_midline", 45.0));
        ruleCfg.soilLowerBound = static_cast<float>(
            cfg.getDouble("control.soil.lower_bound", 35.0));
        ruleCfg.soilUpperBound = static_cast<float>(
            cfg.getDouble("control.soil.upper_bound", 55.0));
        ruleCfg.soilEmergency = static_cast<float>(
            cfg.getDouble("control.soil.emergency", 15.0));

        ruleCfg.vpdUnlockThreshold = static_cast<float>(
            cfg.getDouble("control.vpd.unlock_threshold_kpa", 0.8));
        ruleCfg.vpdStressThreshold = static_cast<float>(
            cfg.getDouble("control.vpd.stress_threshold_kpa", 2.0));
        ruleCfg.vpdHumidityLock = static_cast<float>(
            cfg.getDouble("control.vpd.humidity_lock_kpa", 0.3));

        ruleCfg.noonTempThreshold = static_cast<float>(
            cfg.getDouble("control.noon.temp_threshold_c", 30.0));
        ruleCfg.noonSoilTempThreshold = static_cast<float>(
            cfg.getDouble("control.noon.soil_temp_threshold_c", 28.0));

        ruleCfg.cooldownSeconds = cfg.getInt("control.cooldown.cooldown_seconds", 600);

        ruleCfg.dawnStart      = cfg.getInt("control.time_windows.dawn_start_h", 6);
        ruleCfg.forenoonStart  = cfg.getInt("control.time_windows.forenoon_start_h", 8);
        ruleCfg.noonStart      = cfg.getInt("control.time_windows.noon_start_h", 12);
        ruleCfg.afternoonStart = cfg.getInt("control.time_windows.afternoon_start_h", 14);
        ruleCfg.eveningStart   = cfg.getInt("control.time_windows.evening_start_h", 17);
        ruleCfg.nightStart     = cfg.getInt("control.time_windows.night_start_h", 19);

        controller = std::make_unique<ExpertRuleController>(ruleCfg);
        LOG_INFO("使用专家规则控制器");
    } else {
        controller = std::make_unique<MlController>(
            cfg.getString("control.model_path", ""));
        LOG_INFO("使用 ML 控制器 (model=%s)",
                 cfg.getString("control.model_path", "none"));
    }

    // ---- 创建 CSV 写入器 ----
    CsvWriter csvWriter(
        cfg.getString("csv.output_dir", "data"),
        cfg.getInt("csv.flush_interval_ms", 5000),
        cfg.getInt("csv.buffer_max_rows", 100));

    // ---- 启动管道 ----
    Pipeline pipeline(uartFd, &csvWriter, std::move(controller),
                     cfg.getInt("control.sensor_buffer_ms", 60000));
    pipeline.start();
    LOG_INFO("管道已启动，等待数据...");

    // ---- 等待退出信号 ----
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ---- 关闭 ----
    LOG_INFO("正在关闭...");
    pipeline.stop();
    pipeline.join();
    csvWriter.close();
    uart_close(uartFd);
    Logger::instance().shutdown();
    LOG_INFO("关闭完成");
    return 0;
}
