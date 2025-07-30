#include "AIEngine.hpp"
#include "ConfigManager.hpp"
#include "MemoryManager.hpp"
#include "SessionManager.hpp"
#include "Logger.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <string>
#include <algorithm>

AIEngine::AIEngine(ConfigManager& config, MemoryManager& memory_manager)
    : config_(config), 
      memory_manager_(memory_manager),
      // 使用 [API_LLM] 部分的Key初始化llmHttpClient_
      llmHttpClient_(config.get("API_LLM", "DEEPSEEK_API_KEY")),
      // 使用 [API_EMBEDDING] 部分的Key初始化embeddingHttpClient_
      embeddingHttpClient_(config.get("API_EMBEDDING", "EMBEDDING_API_KEY"))
{
    // 从 [API_LLM] 加载聊天模型配置
    llm_model_ = config.get("AI", "MODEL", "deepseek-chat");
    llm_api_url_ = config.get("API_LLM", "API_BASE_URL", "https://api.deepseek.com/v1");
    
    // 从 [API_EMBEDDING] 加载Embedding模型配置
    embedding_model_ = config.get("API_EMBEDDING", "EMBEDDING_MODEL", "");
    embedding_api_url_ = config.get("API_EMBEDDING", "EMBEDDING_API_URL", "");

    // 加载通用配置
    temperature_ = std::stof(config.get("AI", "TEMPERATURE", "0.7"));
    std::string rag_flag_str = config.get("AI", "ENABLE_RAG", "false");
    std::transform(rag_flag_str.begin(), rag_flag_str.end(), rag_flag_str.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    rag_enabled_ = (rag_flag_str == "true");

    if (rag_enabled_) {
        Logger::logInfo("AIEngine 已初始化。RAG记忆系统: [已启用]");
    } else {
        Logger::logInfo("AIEngine 已初始化。RAG记忆系统: [已禁用]");
    }
}

std::string AIEngine::processPlayerInput(const std::string& user_input, SessionManager& session) {
    if (rag_enabled_) {
        // --- RAG 启用路径 (有记忆) ---
        Logger::logInfo("开始处理玩家输入 (RAG路径)...");

        auto query_embedding = getEmbeddings(user_input);
        std::vector<std::string> retrieved_memories = memory_manager_.retrieveMemories(query_embedding, 3);

        std::string system_prompt_template;
        const std::string prompt_file_path = config_.get("SystemPrompt", "PROMPT_FILE", "prompt.txt");
        if (!prompt_file_path.empty()) {
            std::ifstream file(prompt_file_path);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                system_prompt_template = buffer.str();
            }
        }
        
        std::string memory_section = "无相关记忆。";
        if (!retrieved_memories.empty()) {
            std::stringstream ss;
            for (const auto& mem : retrieved_memories) {
                ss << "- " << mem << "\n";
            }
            memory_section = ss.str();
        }
        size_t placeholder_pos = system_prompt_template.find("[CONVERSATION_MEMORY]");
        if (placeholder_pos != std::string::npos) {
            system_prompt_template.replace(placeholder_pos, std::string("[CONVERSATION_MEMORY]").length(), memory_section);
        }

        nlohmann::json messages_payload = nlohmann::json::array();
        messages_payload.push_back({{"role", "system"}, {"content", system_prompt_template}});
        
        const auto& history = session.getHistory();
        for (const auto& msg : history) {
            messages_payload.push_back(msg);
        }

        std::string ai_response = generateResponse(messages_payload);

        std::string summary = createMemorySummary(user_input, ai_response);
        auto summary_embedding = getEmbeddings(summary);
        memory_manager_.addMemory(summary, summary_embedding);
        
        return ai_response;

    } else {
        // --- RAG 禁用路径 (无记忆) ---
        Logger::logInfo("开始处理玩家输入 (非RAG路径)...");

        std::string system_prompt;
        const std::string prompt_file_path = config_.get("SystemPrompt", "PROMPT_FILE", "prompt.txt");
        if (!prompt_file_path.empty()) {
            std::ifstream file(prompt_file_path);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                system_prompt = buffer.str();
            }
        }
        
        nlohmann::json messages_payload = nlohmann::json::array();
        messages_payload.push_back({{"role", "system"}, {"content", system_prompt}});
        
        const auto& history = session.getHistory();
        for (const auto& msg : history) {
            messages_payload.push_back(msg);
        }

        return generateResponse(messages_payload);
    }
}

std::string AIEngine::generateResponse(const nlohmann::json& messages_payload) {
    nlohmann::json payload = {
        {"model", llm_model_},
        {"messages", messages_payload},
        {"temperature", temperature_}
    };
    std::cout << "[调试] LLM 请求负载:\n" << payload.dump(2) << std::endl;
    // 使用专门的 llmHttpClient_
    std::string response = llmHttpClient_.post(llm_api_url_ + "/chat/completions", payload.dump());
    try {
        auto response_json = nlohmann::json::parse(response);
        if (response_json.contains("error")) {
            throw std::runtime_error("API错误: " + response_json["error"].value("message", "未知API错误"));
        }
        return response_json["choices"][0]["message"]["content"].get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("LLM响应解析失败: " + std::string(e.what()));
    }
}

std::vector<float> AIEngine::getEmbeddings(const std::string& text) {
    // 防御性检查：确保URL已被配置
    if (embedding_api_url_.empty()) {
        Logger::logError("Embedding API URL 未在.env文件的[API_EMBEDDING]节中配置，无法获取向量！");
        int dim = std::stoi(config_.get("API_EMBEDDING", "EMBEDDING_VECTOR_DIMENSION", "1024"));
        return std::vector<float>(dim, 0.0f);
    }

    nlohmann::json payload = {
        {"model", embedding_model_},
        {"input", text}
    };
    
    std::string full_url = embedding_api_url_;
    // 许多API的endpoint是 /embeddings，但有些可能直接就是基础URL。
    // 为提高兼容性，我们假设基础URL后可能需要拼接/embeddings
    // 更稳健的做法是让用户在.env中配置完整的URL
    if (full_url.find("/embeddings") == std::string::npos) {
         if (full_url.back() != '/') {
            full_url += "/";
        }
        full_url += "embeddings";
    }

    // 使用专门的 embeddingHttpClient_
    std::string response_str = embeddingHttpClient_.post(full_url, payload.dump());

    try {
        auto response_json = nlohmann::json::parse(response_str);
        if (response_json.contains("data") && !response_json["data"].empty() && response_json["data"][0].contains("embedding")) {
            return response_json["data"][0]["embedding"].get<std::vector<float>>();
        }
        // 尝试解析可能的错误信息
        if (response_json.contains("error") && response_json["error"].is_object()) {
             throw std::runtime_error("Embedding API 返回错误: " + response_json["error"].value("message", "未知错误"));
        }
        throw std::runtime_error("Embedding 响应格式无效。");
    } catch(const std::exception& e) {
        Logger::logError("获取 Embedding 失败: " + std::string(e.what()));
        int dim = std::stoi(config_.get("API_EMBEDDING", "EMBEDDING_VECTOR_DIMENSION", "1024"));
        return std::vector<float>(dim, 0.0f);
    }
}

std::string AIEngine::createMemorySummary(const std::string& input, const std::string& response) {
    std::string clean_response = response;
    try {
        std::regex re_main("【.+?】(.+?)<.+?>");
        std::smatch match;
        if(std::regex_search(response, match, re_main) && match.size() > 1) {
            clean_response = match[1].str();
        }
    } catch (...) {}
    return "玩家说：'" + input + "'，我的回应是：'" + clean_response + "'";
}

std::string AIEngine::synthesizeSpeech(const std::string& text_jp, const std::string& voice_api_url) {
    // 假设语音合成是另一个独立的简单服务，所以临时创建一个客户端
    if (voice_api_url.empty() || text_jp.empty()) return "";
    nlohmann::json payload = {{"text", text_jp}, {"speaker_id", 0}};
    try {
        HTTPClient ttsClient;
        std::string response = ttsClient.post(voice_api_url, payload.dump());
        return nlohmann::json::parse(response).value("audio_url", "");
    } catch (const std::exception& e) {
        Logger::logError("语音合成失败: " + std::string(e.what()));
        return "";
    }
}