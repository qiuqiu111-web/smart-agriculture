#pragma once

#include <string>

// 轻量 JSON 配置读取器 —— 仅处理 config.json 中用到的结构
class JsonConfig {
public:
    bool load(const std::string& filepath);

    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int         getInt(const std::string& key, int defaultValue = 0) const;
    double      getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool        getBool(const std::string& key, bool defaultValue = false) const;

private:
    // 按点分隔的 key 路径（如 "uart.device"）在 JSON 文本中导航，
    // 返回原始值子串，失败返回空串
    std::string findRawValue(const std::string& dotPath) const;

    // 从 scope 中提取 pos 位置开始的单个 JSON 值
    static std::string extractValue(const std::string& scope, size_t pos);

    std::string raw_;  // 完整的 JSON 文本
};
