#include "AIEngine.hpp"
#include <stdexcept>
#include <iostream>

AIEngine::AIEngine(const std::string& api_key,
                 const std::string& model,
                 const std::string& api_url)
    : httpClient_(api_key), model_(model), api_url_(api_url) {
    std::cout << "[信息] AIEngine 已初始化，模型: " << model_ << ", API URL: " << api_url_ << std::endl;
}

std::string AIEngine::generateResponse(const nlohmann::json& messages_payload) {
    nlohmann::json payload = {
        {"model", model_},
        {"messages", messages_payload},
        {"temperature", temperature_}
    };

    std::cout << "[调试] AI 请求负载:\n" << payload.dump(2) << std::endl;
    std::string response = httpClient_.post(api_url_ + "/chat/completions", payload.dump());

    try {
        auto response_json = nlohmann::json::parse(response);
        if (response_json.contains("error")) {
            throw std::runtime_error("API错误: " + response_json["error"].value("message", "未知API错误"));
        }
        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            throw std::runtime_error("API 响应中缺少 'choices' 字段。");
        }
        return response_json["choices"][0]["message"]["content"].get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("响应解析失败: " + std::string(e.what()) + "\n原始响应: " + response);
    }
}

// 语音合成功能的实现
std::string AIEngine::synthesizeSpeech(const std::string& text_jp, const std::string& voice_api_url) {
    if (voice_api_url.empty() || text_jp.empty()) {
        return ""; // 如果没有配置API或文本为空，直接返回
    }

    // 构建发送给语音API的JSON体
    // 注意：这个结构需要根据您的 "simple-voist-api" 的要求来调整
    nlohmann::json payload = {
        {"text", text_jp},
        {"speaker_id", 0} // 假设角色 '灵' 的 voice id 是 0
    };

    std::cout << "[信息] 正在调用语音合成API: " << voice_api_url << std::endl;
    try {
        // 使用一个独立的HTTPClient实例或复用现有的来发送请求
        // 这里我们简单地 new 一个，但更好的做法是 AIEngine 持有两个客户端或配置不同的 key/header
        HTTPClient ttsClient;
        std::string response = ttsClient.post(voice_api_url, payload.dump());
        
        auto response_json = nlohmann::json::parse(response);
        // 同样，这里的 "audio_url" 或 "audio_base64" 需要根据您的API的实际返回来调整
        return response_json.value("audio_url", "");
    } catch (const std::exception& e) {
        std::cerr << "[错误] 语音合成失败: " << e.what() << std::endl;
        return ""; // 失败时返回空
    }
}