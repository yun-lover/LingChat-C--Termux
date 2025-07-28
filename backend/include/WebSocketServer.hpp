#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "civetweb.h"
#include "SessionManager.hpp"
#include "MemoryManager.hpp"
#include "AIEngine.hpp"
#include <string>
#include <mutex>

// 前向声明 ConfigManager，因为它通过引用传入构造函数
class ConfigManager;

class WebSocketServer {
public:
    /**
     * @brief 构造函数。
     * 接收 ConfigManager 的引用，并负责创建和拥有所有核心服务。
     * @param config 配置管理器的引用。
     */
    WebSocketServer(ConfigManager& config);
    ~WebSocketServer();

    void start();
    void stop();
    mg_context* get_context() const { return ctx_; }

private:
    // civetweb 的回调处理函数
    int handle_websocket_connect(const mg_connection* conn);
    void handle_websocket_ready(mg_connection* conn);
    int handle_websocket_data(mg_connection* conn, int flags, char* data, size_t data_len);
    void handle_websocket_close(const mg_connection* conn);

    // 静态回调函数，用于将 C 风格的回调映射到 C++ 成员函数
    static int websocket_connect_handler(const mg_connection* conn, void* ws_server_ptr);
    static void websocket_ready_handler(mg_connection* conn, void* ws_server_ptr);
    static int websocket_data_handler(mg_connection* conn, int flags, char* data, size_t data_len, void* ws_server_ptr);
    static void websocket_close_handler(const mg_connection* conn, void* ws_server_ptr);
    static int civetweb_error_log_handler(const mg_connection* conn, const char* message);

    // 内部工具函数
    void send_websocket_message(mg_connection* conn, const std::string& message);
    void log_info(const std::string& message) const;
    void log_error(const std::string& message) const;
    void log_warning(const std::string& message) const;

    mg_context* ctx_ = nullptr;
    ConfigManager& config_;

    // 核心服务成员变量
    // WebSocketServer 现在拥有核心服务对象。
    MemoryManager memory_manager_;
    AIEngine engine_;
    SessionManager session_manager_; 

    // 连接状态管理
    mg_connection* active_connection_ptr_ = nullptr;
    mutable std::mutex connection_mutex_;
};

#endif // WEBSOCKET_SERVER_HPP