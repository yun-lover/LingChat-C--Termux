#include "Logger.hpp"

// 这里是 logInfo 的唯一实现
void Logger::logInfo(const std::string& message) {
    std::cout << "[信息] " << message << std::endl;
}

// 这里是 logError 的唯一实现
void Logger::logError(const std::string& message) {
    std::cerr << "[错误] " << message << std::endl;
}