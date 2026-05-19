#include "control/controller.hpp"
#include "utils/logger.hpp"

MlController::MlController(const std::string& modelPath)
    : modelPath_(modelPath) {}

DecisionResult MlController::decide(const SensorData& /*sensor*/) {
    if (!warned_) {
        LOG_WARN("MlController: 未加载模型，返回全关命令");
        warned_ = true;
    }
    return DecisionResult{};
}
