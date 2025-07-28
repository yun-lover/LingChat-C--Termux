#ifndef AI_ENGINE_HPP
#define AI_ENGINE_HPP

#include "HTTPClient.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// 前向声明，避免循环包含
class MemoryManager;
class SessionManager;
class ConfigManager;

class AIEngine {
public:
    /**
     * @brief 构造函数。
     * @param config 配置管理器的引用。
     * @param memory_manager 记忆管理器的引用。
     */
    AIEngine(ConfigManager& config, MemoryManager& memory_manager);
    
    /**
     * @brief RAG流程的总入口点。
     * 根据配置决定是否启用记忆功能，处理玩家输入并返回AI的最终回复。
     * @param user_input 玩家的原始输入文本。
     * @param session 当前的会话管理器，用于获取短期历史。
     * @return AI生成的回复。
     */
    std::string processPlayerInput(const std::string& user_input, SessionManager& session);
    
    /**
     * @brief 调用语音合成API。
     * @param text_jp 要合成的日文文本。
     * @param voice_api_url 语音API的URL。
     * @return 合成后的音频URL或空字符串。
     */
    std::string synthesizeSpeech(const std::string& text_jp, 
                                 const std::string& voice_api_url);

private:
    // 私有辅助方法
    std::string generateResponse(const nlohmann::json& messages_payload);
    std::vector<float> getEmbeddings(const std::string& text);
    std::string createMemorySummary(const std::string& input, const std::string& response);

    // 依赖和配置
    ConfigManager& config_;
    MemoryManager& memory_manager_;
    HTTPClient httpClient_;

    // 从配置加载的参数
    std::string llm_model_;
    std::string llm_api_url_;
    std::string embedding_model_;
    std::string embedding_api_url_;
    float temperature_;
    
    // RAG功能开关
    bool rag_enabled_ = false;
};

#endif // AI_ENGINE_HPP