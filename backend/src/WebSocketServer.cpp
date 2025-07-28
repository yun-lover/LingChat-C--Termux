#include "WebSocketServer.hpp"
#include "ConfigManager.hpp"
#include "Logger.hpp"

#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <limits.h>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream> 

// 辅助函数：去除字符串首尾的空白字符
static std::string trim_string(const std::string& s) {
    const char* ws = " \t\n\r\f\v";
    size_t first = s.find_first_not_of(ws);
    if (std::string::npos == first) return "";
    size_t last = s.find_last_not_of(ws);
    return s.substr(first, (last - first + 1));
}

// CivetWeb回调函数转发器
int WebSocketServer::websocket_connect_handler(const mg_connection* conn, void* ws_server_ptr) { 
    return static_cast<WebSocketServer*>(ws_server_ptr)->handle_websocket_connect(conn); 
}
void WebSocketServer::websocket_ready_handler(mg_connection* conn, void* ws_server_ptr) { 
    static_cast<WebSocketServer*>(ws_server_ptr)->handle_websocket_ready(conn); 
}
int WebSocketServer::websocket_data_handler(mg_connection* conn, int flags, char* data, size_t data_len, void* ws_server_ptr) { 
    return static_cast<WebSocketServer*>(ws_server_ptr)->handle_websocket_data(conn, flags, data, data_len); 
}
void WebSocketServer::websocket_close_handler(const mg_connection* conn, void* ws_server_ptr) { 
    static_cast<WebSocketServer*>(ws_server_ptr)->handle_websocket_close(conn); 
}
int WebSocketServer::civetweb_error_log_handler(const mg_connection *conn, const char *message) { 
    std::string msg = message; 
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    std::cerr << "[CivetWeb Internal Error] " << msg << std::endl;
    return 0; 
}

// 构造函数：初始化所有核心服务
WebSocketServer::WebSocketServer(ConfigManager& config) 
    : config_(config), 
      // 初始化 MemoryManager，使用.env中定义的文件路径
      memory_manager_(config.get("Database", "MEMORY_DB_PATH", "memory.json")),
      // 将 config 和 memory_manager 传入 AIEngine
      engine_(config, memory_manager_), 
      // 初始化 SessionManager
      session_manager_(std::stoul(config.get("AI", "MAX_HISTORY_TURNS", "10"))) 
{
    log_info("WebSocketServer 已初始化。");
}

WebSocketServer::~WebSocketServer() { 
    stop(); 
    log_info("WebSocketServer 已销毁。"); 
}

void WebSocketServer::start() {
    if (ctx_) { 
        log_error("WebSocketServer 已经在运行中。"); 
        return; 
    }
    std::string port_str = config_.get("Server", "BACKEND_PORT", "8765");
    std::string doc_root = config_.get("Server", "DOCUMENT_ROOT", "frontend");
    char current_path_buffer[PATH_MAX];
    if (getcwd(current_path_buffer, sizeof(current_path_buffer)) == nullptr) { 
        throw std::runtime_error("无法获取当前工作目录。"); 
    }
    std::string absolute_document_root = std::string(current_path_buffer) + "/" + doc_root;
    const char* options[] = { 
        "listening_ports", port_str.c_str(), 
        "document_root", absolute_document_root.c_str(), 
        "num_threads", "5", 
        nullptr 
    };
    log_info("正在启动 WebSocket 服务器于 " + port_str + "...");
    log_info("正在从以下目录提供静态文件: " + absolute_document_root);
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.log_message = civetweb_error_log_handler;
    ctx_ = mg_start(&callbacks, this, options);
    if (!ctx_) throw std::runtime_error("CivetWeb 服务器启动失败。");
    log_info("WebSocket 服务器启动成功。");
    mg_set_websocket_handler(
        ctx_, "/websocket", 
        websocket_connect_handler, websocket_ready_handler, 
        websocket_data_handler, websocket_close_handler, this
    );
    log_info("WebSocket 端点已注册: /websocket");
}

void WebSocketServer::stop() { 
    if (ctx_) { 
        log_info("正在停止 WebSocket 服务器..."); 
        mg_stop(ctx_); 
        ctx_ = nullptr; 
        std::lock_guard<std::mutex> lock(connection_mutex_); 
        active_connection_ptr_ = nullptr; 
        log_info("WebSocket 服务器已停止。"); 
    } 
}

int WebSocketServer::handle_websocket_connect(const mg_connection* conn) { 
    std::lock_guard<std::mutex> lock(connection_mutex_); 
    if (active_connection_ptr_ != nullptr) { 
        log_info("拒绝新连接: 已有活跃连接存在。"); 
        return 1;
    } 
    log_info("新 WebSocket 连接请求已授权，等待就绪..."); 
    return 0;
}

void WebSocketServer::handle_websocket_close(const mg_connection* conn) { 
    const struct mg_request_info *ri = mg_get_request_info(conn); 
    log_info("WebSocket 连接正在关闭: " + std::string(ri->request_uri)); 
    std::lock_guard<std::mutex> lock(connection_mutex_); 
    if (active_connection_ptr_ == conn) { 
        active_connection_ptr_ = nullptr; 
        log_info("活跃连接已清空。服务器现在可以接受新连接。"); 
    } 
}

void WebSocketServer::handle_websocket_ready(mg_connection* conn) {
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        active_connection_ptr_ = conn;
    }
    session_manager_.clearHistory();
    log_info("WebSocket 连接已就绪，并已清空会话历史。");
    nlohmann::json ready_msg = {
        {"type", "server_ready"},
        {"payload", {
            {"character_name", config_.get("Character", "CHARACTER_NAME", "AI")},
            {"character_identity", config_.get("Character", "CHARACTER_IDENTITY", "助手")},
            {"ui_config", {
                {"background_day", config_.get("UI", "BACKGROUND_DAY_PATH", "")},
                {"background_night", config_.get("UI", "BACKGROUND_NIGHT_PATH", "")},
                {"character_sprite_dir", config_.get("UI", "CHARACTER_SPRITE_DIR", "")},
                {"emotion_map", config_.getSection("EmotionMap")},
                {"sfx_dir", config_.get("UI", "SFX_DIR", "assets/sfx/")},
                {"emotion_sfx_map", config_.getSection("EmotionSfxMap")}
            }}
        }}
    };
    send_websocket_message(conn, ready_msg.dump());
}

int WebSocketServer::handle_websocket_data(mg_connection* conn, int flags, char* data, size_t data_len) {
    if (!(flags & MG_WEBSOCKET_OPCODE_TEXT)) return 1;
    
    std::string received_data(data, data_len);
    try {
        auto json_msg = nlohmann::json::parse(received_data);
        std::string msg_type = json_msg.value("type", "unknown");
        std::string user_input;

        if (msg_type == "user_message") {
            user_input = json_msg["payload"].value("text", "");
        } else if (msg_type == "system_command") {
            std::string command = json_msg["payload"].value("command", "");
            std::string value = json_msg["payload"].value("value", "");
            user_input = "{指令：" + command + " " + value + "}";
        } else {
            return 1;
        }
        
        if (user_input.empty()) return 1;

        session_manager_.addMessage("user", user_input);
        
        log_info("正在调用 AI 引擎...");
        std::string ai_raw_response = engine_.processPlayerInput(user_input, session_manager_);
        log_info("AI 引擎返回: " + ai_raw_response);
        
        session_manager_.addMessage("assistant", ai_raw_response);

        nlohmann::json segments = nlohmann::json::array();
        std::regex re_main("【(.+?)】(.+?)<(.+?)>");
        
        auto sentences_begin = std::sregex_iterator(ai_raw_response.begin(), ai_raw_response.end(), re_main);
        auto sentences_end = std::sregex_iterator();

        for (std::sregex_iterator i = sentences_begin; i != sentences_end; ++i) {
            std::smatch match = *i;
            if (match.size() < 4) continue;

            std::string expression = trim_string(match[1].str());
            std::string middle_content = trim_string(match[2].str());
            std::string text_jp = trim_string(match[3].str());
            std::string action = "";
            std::string text_cn = "";

            std::smatch action_match;
            std::regex re_action("\\((.+?)\\)");
            if (std::regex_search(middle_content, action_match, re_action) && action_match.size() > 1) {
                action = trim_string(action_match[1].str());
                text_cn = trim_string(std::regex_replace(middle_content, re_action, ""));
            } else {
                text_cn = middle_content;
            }

            std::string audio_url = engine_.synthesizeSpeech(
                text_jp, config_.get("API", "VOICE_API_URL", "")
            );
            segments.push_back({
                {"expression", expression}, {"action", action},
                {"text_cn", text_cn}, {"audio_url", audio_url}
            });
        }

        if (segments.empty()) {
            log_warning("无法从AI响应中解析出格式化片段，将返回原始文本。");
            segments.push_back({
                {"expression", "default"}, {"action", ""}, 
                {"text_cn", ai_raw_response}, {"audio_url", ""}
            });
        }
        
        nlohmann::json response_to_frontend = {
            {"type", (msg_type == "system_command") ? "narration" : "ai_response"}, 
            {"payload", {{"segments", segments}}}
        };
        send_websocket_message(conn, response_to_frontend.dump());

    } catch (const std::exception& e) {
        log_error("处理数据时发生错误: " + std::string(e.what()));
    }
    return 1;
}

void WebSocketServer::send_websocket_message(mg_connection* conn, const std::string& message) { 
    if (conn) { 
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, message.c_str(), message.length()); 
        log_info("发送 WebSocket 数据: " + message.substr(0, 200) + "...");
    } 
}

void WebSocketServer::log_info(const std::string& message) const { 
    std::cout << "[信息] WebSocketServer: " << message << std::endl; 
}
void WebSocketServer::log_error(const std::string& message) const { 
    std::cerr << "[错误] WebSocketServer: " << message << std::endl; 
}
void WebSocketServer::log_warning(const std::string& message) const { 
    std::cerr << "[警告] WebSocketServer: " << message << std::endl; 
}