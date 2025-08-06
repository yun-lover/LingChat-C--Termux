#include "ConfigManager.hpp"
#include "WebSocketServer.hpp"
#include "civetweb.h"

#include <iostream>
#include <stdexcept>
#include <csignal>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string>

static volatile bool g_exit_flag = false;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\n[信息] 收到停止信号，准备关闭服务器..." << std::endl;
        g_exit_flag = true;
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    unsigned initialized_features = mg_init_library(MG_FEATURES_SSL | MG_FEATURES_WEBSOCKET);
    if (!(initialized_features & MG_FEATURES_WEBSOCKET)) {
        std::cerr << "[致命错误] CivetWeb 未能初始化 WebSocket 功能，程序无法启动。" << std::endl;
        return 1;
    }
    std::cout << "[信息] CivetWeb 库已初始化。" << std::endl;

    try {
        // 0. 加载配置
        ConfigManager config_manager;

        // 2. 检查主聊天LLM的 API 密钥 (快速失败机制)
        const std::string api_key = config_manager.get("API_LLM", "DEEPSEEK_API_KEY", "");
        if (api_key.empty() || api_key == "your_deepseek_api_key_here") {
            std::cerr << "[致命错误] DEEPSEEK_API_KEY 未在 .env 文件的 [API_LLM] 节中设置或无效！" << std::endl;
            mg_exit_library();
            return 1;
        }

        // 2.1. 如果RAG已启用，则检查Embedding模型的API密钥
        std::string rag_flag_str = config_manager.get("AI", "ENABLE_RAG", "false");
        std::transform(rag_flag_str.begin(), rag_flag_str.end(), rag_flag_str.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        if (rag_flag_str == "true") {
            const std::string embedding_api_key = config_manager.get("API_EMBEDDING", "EMBEDDING_API_KEY", "");
            if (embedding_api_key.empty() || embedding_api_key == "your_embedding_api_key_here") {
                std::cerr << "[致命错误] RAG已启用，但 EMBEDDING_API_KEY 未在 .env 文件的 [API_EMBEDDING] 节中设置或无效！" << std::endl;
                mg_exit_library();
                return 1;
            }
        }
        
        // 3. 初始化并启动 WebSocket 服务器
        WebSocketServer ws_server(config_manager);
        ws_server.start();

        std::cout << "[信息] 服务器已成功启动。主线程进入等待模式。按 Ctrl+C 关闭。" << std::endl;

        while (!g_exit_flag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "[信息] 服务器主循环已退出，正在清理..." << std::endl;

        ws_server.stop();

    } catch (const std::exception& e) {
        std::cerr << "[致命错误] 主程序异常退出: " << e.what() << std::endl;
    }

    mg_exit_library();
    std::cout << "[信息] 程序已优雅关闭。" << std::endl;
    return 0;
}