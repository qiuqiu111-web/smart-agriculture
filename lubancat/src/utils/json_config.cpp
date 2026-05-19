#include "utils/json_config.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

bool JsonConfig::load(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs) return false;

    std::ostringstream oss;
    oss << ifs.rdbuf();
    raw_ = oss.str();
    return !raw_.empty();
}

std::string JsonConfig::getString(const std::string& key, const std::string& defaultValue) const {
    std::string v = findRawValue(key);
    if (v.empty()) return defaultValue;
    // 去掉首尾引号
    if (v.front() == '"' && v.back() == '"') {
        return v.substr(1, v.length() - 2);
    }
    return v;
}

int JsonConfig::getInt(const std::string& key, int defaultValue) const {
    std::string v = findRawValue(key);
    if (v.empty()) return defaultValue;
    return std::stoi(v);
}

double JsonConfig::getDouble(const std::string& key, double defaultValue) const {
    std::string v = findRawValue(key);
    if (v.empty()) return defaultValue;
    return std::stod(v);
}

bool JsonConfig::getBool(const std::string& key, bool defaultValue) const {
    std::string v = findRawValue(key);
    if (v.empty()) return defaultValue;
    return v == "true";
}

// ========== 内部辅助函数 ==========

std::string JsonConfig::extractValue(const std::string& scope, size_t pos) {
    // 跳过空白字符
    while (pos < scope.length() && (scope[pos] == ' ' || scope[pos] == '\t'
                                    || scope[pos] == '\n' || scope[pos] == '\r'))
        ++pos;
    if (pos >= scope.length()) return "";

    if (scope[pos] == '"') {
        // 字符串值
        size_t end = pos + 1;
        while (end < scope.length() && scope[end] != '"') ++end;
        return scope.substr(pos, end - pos + 1);
    }

    // 数字或布尔值
    size_t end = pos;
    while (end < scope.length()
           && scope[end] != ','
           && scope[end] != '}'
           && scope[end] != ']'
           && scope[end] != '\n'
           && scope[end] != '\r') {
        ++end;
    }
    std::string val = scope.substr(pos, end - pos);
    // 去掉尾部空白
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
        val.pop_back();
    return val;
}

std::string JsonConfig::findRawValue(const std::string& dotPath) const {
    std::string scope = raw_;

    size_t segStart = 0;
    while (segStart < dotPath.length()) {
        // 取下一段 key
        size_t dot = dotPath.find('.', segStart);
        std::string seg = (dot == std::string::npos)
                              ? dotPath.substr(segStart)
                              : dotPath.substr(segStart, dot - segStart);

        // 在当前作用域中搜索 "seg"
        std::string pattern = "\"" + seg + "\"";
        size_t pos = scope.find(pattern);
        if (pos == std::string::npos) return "";

        // 找到 key 后面的 ':'
        pos = scope.find(':', pos + pattern.length());
        if (pos == std::string::npos) return "";
        ++pos;

        if (dot == std::string::npos) {
            // 末段 → 提取值
            return extractValue(scope, pos);
        }

        // 进入嵌套对象
        while (pos < scope.length() && scope[pos] != '{') ++pos;
        if (pos >= scope.length()) return "";

        // 匹配花括号确定对象边界
        int depth = 1;
        size_t objStart = pos;
        ++pos;
        while (pos < scope.length() && depth > 0) {
            if (scope[pos] == '{') ++depth;
            else if (scope[pos] == '}') --depth;
            ++pos;
        }
        scope = scope.substr(objStart, pos - objStart);
        segStart = dot + 1;
    }
    return "";
}
