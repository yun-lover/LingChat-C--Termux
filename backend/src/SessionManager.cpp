#include "SessionManager.hpp"
#include <iostream>

// 实现新的构造函数
SessionManager::SessionManager(size_t max_history) : max_history_size_(max_history) {
    std::cout << "[信息] SessionManager 已初始化，最大对话历史记录: " << max_history_size_ << " 轮" << std::endl;
}

void SessionManager::addMessage(const std::string& role, const std::string& content) {
    nlohmann::json message = {
        {"role", role},
        {"content", content}
    };
    history_.push_back(message);

    // ================= 【核心滑动窗口逻辑】 =================
    // 对话历史包含用户和AI的消息，所以消息总数的限制是 (轮数 * 2)
    // 当消息总数超过这个限制时，从前面开始删除最旧的消息。
    // 使用 while 循环确保即使一次性添加多条消息也能正确处理。
    while (history_.size() > (max_history_size_ * 2)) {
        // history_.erase(history_.begin()) 会高效地移除向量的第一个元素。
        history_.erase(history_.begin()); 
        std::cout << "[调试] 对话历史过长，已移除最旧的一条消息。" << std::endl;
    }
    // ======================================================
}

const std::vector<nlohmann::json>& SessionManager::getHistory() const {
    return history_;
}

nlohmann::json SessionManager::getHistoryAsJson() const {
    nlohmann::json j(history_);
    return j;
}

void SessionManager::clearHistory() {
    history_.clear();
}