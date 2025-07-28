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
      httpClient_(config.get("API", "DEEPSEEK_API_KEY")) 
{
    // 加载API和模型配置
    llm_model_ = config.get("AI", "MODEL", "deepseek-chat");
    llm_api_url_ = config.get("API", "API_BASE_URL", "https://api.deepseek.com/v1");
    embedding_model_ = config.get("API", "EMBEDDING_MODEL", "text-embedding-v2");
    embedding_api_url_ = config.get("API", "EMBEDDING_API_URL", llm_api_url_);
    temperature_ = std::stof(config.get("AI", "TEMPERATURE", "0.7"));
    
    // 从配置文件读取RAG开关状态
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
        
        // ======================= FIX #1 =======================
        // 错误原因：不能直接用vector的迭代器插入到json对象
        // 修正方法：使用循环逐个添加
        const auto& history = session.getHistory();
        for (const auto& msg : history) {
            messages_payload.push_back(msg);
        }
        // ======================================================

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
        
        // ======================= FIX #2 =======================
        // 此处是同样的问题
        const auto& history = session.getHistory();
        for (const auto& msg : history) {
            messages_payload.push_back(msg);
        }
        // ======================================================

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
    std::string response = httpClient_.post(llm_api_url_ + "/chat/completions", payload.dump());
    try {
        auto response_json = nlohmann::json::parse(response);
        if (response_json.contains("error")) {
            throw std::runtime_error("API错误: " + response_json["error"].value("message", "未知API错误"));
        }
        return response_json["choices"][0]["message"]["content"].get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("响应解析失败: " + std::string(e.what()));
    }
}

std::vector<float> AIEngine::getEmbeddings(const std::string& text) {
    nlohmann::json payload = {
        {"model", embedding_model_},
        {"input", text}
    };
    std::string response_str = httpClient_.post(embedding_api_url_ + "/embeddings", payload.dump());
    try {
        auto response_json = nlohmann::json::parse(response_str);
        if (response_json.contains("data") && !response_json["data"].empty() && response_json["data"][0].contains("embedding")) {
            return response_json["data"][0]["embedding"].get<std::vector<float>>();
        }
        throw std::runtime_error("Embedding 响应格式无效。");
    } catch(const std::exception& e) {
        Logger::logError("获取 Embedding 失败: " + std::string(e.what()));
        int dim = std::stoi(config_.get("API", "EMBEDDING_VECTOR_DIMENSION", "1024"));
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