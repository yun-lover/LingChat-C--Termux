#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <vector>
#include <nlohmann/json.hpp>

class SessionManager {
public:
    /**
     * @brief 构造函数
     * @param max_history 设置要保留的最近对话轮数 (1轮 = 1用户 + 1助手)。默认为10轮。
     */
    SessionManager(size_t max_history = 10); 

    void addMessage(const std::string& role, const std::string& content);
    const std::vector<nlohmann::json>& getHistory() const;
    nlohmann::json getHistoryAsJson() const;
    void clearHistory();

private:
    std::vector<nlohmann::json> history_;
    size_t max_history_size_; // 存储历史记录的最大轮数
};

#endif // SESSION_MANAGER_HPP