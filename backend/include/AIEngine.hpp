#ifndef AI_ENGINE_HPP
#define AI_ENGINE_HPP

#include "HTTPClient.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// 前向声明
class MemoryManager;
class SessionManager;
class ConfigManager;

class AIEngine {
public:
    AIEngine(ConfigManager& config, MemoryManager& memory_manager);
    
    std::string processPlayerInput(const std::string& user_input, SessionManager& session);
    std::string synthesizeSpeech(const std::string& text_jp, 
                                 const std::string& voice_api_url);

private:
    // 私有辅助方法
    std::string generateResponse(const nlohmann::json& messages_payload);
    std::vector<float> getEmbeddings(const std::string& text);
    std::string createMemorySummary(const std::string& input, const std::string& response);

    // 依赖
    ConfigManager& config_;
    MemoryManager& memory_manager_;

    // HTTP客户端：现在有两个，分别用于不同的服务
    HTTPClient llmHttpClient_;
    HTTPClient embeddingHttpClient_;

    // API URL：同样分离
    std::string llm_api_url_;
    std::string embedding_api_url_;
    
    // 其他配置参数
    std::string llm_model_;
    std::string embedding_model_;
    float temperature_;
    bool rag_enabled_ = false;
};

#endif // AI_ENGINE_HPP