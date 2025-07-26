#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "civetweb.h"
#include "SessionManager.hpp" // 【修改】确保包含了头文件
#include <string>
#include <mutex>

// 前向声明以避免循环引用
class ConfigManager;
class AIEngine;

class WebSocketServer {
public:
    WebSocketServer(ConfigManager& config, AIEngine& engine);
    ~WebSocketServer();

    void start();
    void stop();
    mg_context* get_context() const { return ctx_; }

private:
    // ... 其他私有成员和函数 ...
    int handle_websocket_connect(const mg_connection* conn);
    void handle_websocket_ready(mg_connection* conn);
    int handle_websocket_data(mg_connection* conn, int flags, char* data, size_t data_len);
    void handle_websocket_close(const mg_connection* conn);

    static int websocket_connect_handler(const mg_connection* conn, void* ws_server_ptr);
    static void websocket_ready_handler(mg_connection* conn, void* ws_server_ptr);
    static int websocket_data_handler(mg_connection* conn, int flags, char* data, size_t data_len, void* ws_server_ptr);
    static void websocket_close_handler(const mg_connection* conn, void* ws_server_ptr);
    static int civetweb_error_log_handler(const mg_connection* conn, const char* message);

    void send_websocket_message(mg_connection* conn, const std::string& message);
    void log_info(const std::string& message) const;
    void log_error(const std::string& message) const;
    void log_warning(const std::string& message) const;

    mg_context* ctx_ = nullptr;
    ConfigManager& config_;
    AIEngine& engine_;
    SessionManager session_manager_; // 【修改】SessionManager 变为对象成员

    mg_connection* active_connection_ptr_ = nullptr;
    mutable std::mutex connection_mutex_;
};

#endif // WEBSOCKET_SERVER_HPP