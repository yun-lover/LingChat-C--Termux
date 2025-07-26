#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>

/**
 * @brief Logger 类，用于统一的日志输出。
 */
class Logger {
public:
    // **【关键修正】: 只留下函数声明，移除实现**
    static void logInfo(const std::string& message);
    static void logError(const std::string& message);
};

#endif // LOGGER_HPP