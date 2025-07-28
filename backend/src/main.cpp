#include "ConfigManager.hpp"
#include "WebSocketServer.hpp"
#include "civetweb.h"

#include <iostream>
#include <stdexcept>
#include <csignal>
#include <thread>
#include <chrono>

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
        // 1. 加载配置
        ConfigManager config_manager;

        // 2. 检查 API 密钥
        const std::string api_key = config_manager.get("API", "DEEPSEEK_API_KEY", "");
        if (api_key.empty() || api_key == "your_deepseek_api_key_here") {
            std::cerr << "[致命错误] DEEPSEEK_API_KEY 未在 .env 文件中设置或无效！" << std::endl;
            mg_exit_library();
            return 1;
        }
        
        // 3. 初始化并启动 WebSocket 服务器
        // 只需创建 WebSocketServer，它会负责创建其他所有服务。
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