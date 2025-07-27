#ifndef AI_ENGINE_HPP
#define AI_ENGINE_HPP

#include "HTTPClient.hpp"
#include <nlohmann/json.hpp>
#include <string>

class AIEngine {
public:
    AIEngine(const std::string& api_key,
            const std::string& model,
            const std::string& api_url);
    std::string generateResponse(const nlohmann::json& messages_payload);
    std::string synthesizeSpeech(const std::string& text_jp, 
                              const std::string& voice_api_url);

private:
    HTTPClient httpClient_;
    std::string model_;
    std::string api_url_;
    float temperature_ = 0.7;
};

#endif // AI_ENGINE_HPP