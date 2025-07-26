#include "ConfigManager.hpp"
#include "AIEngine.hpp"
#include "WebSocketServer.hpp"
#include "civetweb.h"

#include <iostream>
#include <stdexcept>
#include <csignal> // 用于 std::signal
#include <thread>   // 用于 std::this_thread
#include <chrono>   // 用于 std::chrono

// 全局变量，用于信号处理
static volatile bool g_exit_flag = false;

// 信号处理函数
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\n[信息] 收到停止信号，准备关闭服务器..." << std::endl;
        g_exit_flag = true;
    }
}

int main() {
    // 注册信号处理函数
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 初始化 CivetWeb 库，这是第一步
    unsigned initialized_features = mg_init_library(MG_FEATURES_SSL | MG_FEATURES_WEBSOCKET);
    if (!(initialized_features & MG_FEATURES_SSL)) {
        std::cerr << "[致命错误] CivetWeb 未能初始化 SSL 功能，程序无法启动。" << std::endl;
        return 1;
    }
    std::cout << "[信息] CivetWeb 库已初始化 (SSL+WebSocket)。" << std::endl;

    try {
        // 1. 加载配置
        ConfigManager config_manager;

        // 2. 检查 API 密钥 (快速失败机制)
        const std::string api_key = config_manager.get("API", "DEEPSEEK_API_KEY", "");
        if (api_key.empty() || api_key == "your_deepseek_api_key_here") {
            std::cerr << "[致命错误] DEEPSEEK_API_KEY 未在 .env 文件中设置或无效！请填入有效的密钥后重试。" << std::endl;
            mg_exit_library();
            return 1;
        }
        
        // 3. 初始化 AI 引擎
        const std::string api_url = config_manager.get("API", "API_BASE_URL", "https://api.deepseek.com/v1");
        const std::string model = config_manager.get("AI", "MODEL", "deepseek-chat");
        AIEngine ai_engine(api_key, model, api_url);

        // 4. 初始化并启动 WebSocket 服务器
        WebSocketServer ws_server(config_manager, ai_engine);
        ws_server.start();

        std::cout << "[信息] 服务器已成功启动。主线程进入等待模式。按 Ctrl+C 关闭。" << std::endl;

        // 【核心修复】主循环检查全局退出标志
        while (!g_exit_flag) {
            // 短暂休眠以降低CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "[信息] 服务器主循环已退出，正在清理..." << std::endl;

        // 5. 停止 WebSocket 服务器
        ws_server.stop();

    } catch (const std::exception& e) {
        std::cerr << "[致命错误] 主程序异常退出: " << e.what() << std::endl;
        mg_exit_library();
        return 1;
    }

    // 最后清理 CivetWeb 库
    mg_exit_library();
    std::cout << "[信息] 程序已优雅关闭。" << std::endl;
    return 0;
}