#include "WebSocketServer.hpp"
#include "ConfigManager.hpp"
#include "AIEngine.hpp"
#include "SessionManager.hpp"
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "civetweb.h"
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
    const char* ws = " \t\n\r\f\v";  // 空白字符集合
    size_t first = s.find_first_not_of(ws);  // 第一个非空白字符位置
    if (std::string::npos == first) return "";  // 全空白字符串处理
    size_t last = s.find_last_not_of(ws);  // 最后一个非空白字符位置
    return s.substr(first, (last - first + 1));  // 截取有效子串
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

// CivetWeb错误日志处理器
int WebSocketServer::civetweb_error_log_handler(const mg_connection *conn, const char *message) { 
    std::string msg = message; 
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();  // 去除尾部换行符
    std::cerr << "[CivetWeb Internal Error] " << msg << std::endl; 
    return 0; 
}

// 构造函数：初始化配置、AI引擎和会话管理器
WebSocketServer::WebSocketServer(ConfigManager& config, AIEngine& engine) 
    : config_(config), 
      engine_(engine), 
      // 从配置读取最大历史轮数初始化会话管理器
      session_manager_(std::stoul(config.get("AI", "MAX_HISTORY_TURNS", "10"))) 
{
    log_info("WebSocketServer 已初始化。");
}

// 析构函数：停止服务器并清理资源
WebSocketServer::~WebSocketServer() { 
    stop(); 
    log_info("WebSocketServer 已销毁。"); 
}

// 启动WebSocket服务器
void WebSocketServer::start() {
    if (ctx_) { 
        log_error("WebSocketServer 已经在运行中。"); 
        return; 
    }

    // 获取配置参数
    std::string port_str = config_.get("Server", "BACKEND_PORT", "8765");
    std::string doc_root = config_.get("Server", "DOCUMENT_ROOT", "frontend");
    
    // 获取当前工作目录
    char current_path_buffer[PATH_MAX];
    if (getcwd(current_path_buffer, sizeof(current_path_buffer)) == nullptr) { 
        throw std::runtime_error("无法获取当前工作目录。"); 
    }
    std::string absolute_document_root = std::string(current_path_buffer) + "/" + doc_root;
    
    // 配置服务器选项
    const char* options[] = { 
        "listening_ports", port_str.c_str(), 
        "document_root", absolute_document_root.c_str(), 
        "num_threads", "5", 
        // "error_log_file", "civetweb_errors.log", // 可选：将错误日志重定向到文件
        // "access_log_file", "civetweb_access.log", // 可选：访问日志
        // "log_level", "2", // 关键：设置日志级别 (0=debug, 1=info, 2=warning, 3=error)
        nullptr 
    };
    
    log_info("正在启动 WebSocket 服务器于 " + port_str + "...");
    log_info("正在从以下目录提供静态文件: " + absolute_document_root);
    
    // 设置回调并启动服务器
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.log_message = civetweb_error_log_handler;  // 设置错误日志回调
    ctx_ = mg_start(&callbacks, this, options);
    
    if (!ctx_) throw std::runtime_error("CivetWeb 服务器启动失败。");
    
    log_info("WebSocket 服务器启动成功。");
    
    // 注册WebSocket处理器
    mg_set_websocket_handler(
        ctx_, 
        "/websocket", 
        websocket_connect_handler, 
        websocket_ready_handler, 
        websocket_data_handler, 
        websocket_close_handler, 
        this
    );
    log_info("WebSocket 端点已注册: /websocket");
}

// 停止服务器
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

// 处理新连接请求
int WebSocketServer::handle_websocket_connect(const mg_connection* conn) { 
    std::lock_guard<std::mutex> lock(connection_mutex_); 
    if (active_connection_ptr_ != nullptr) { 
        log_info("拒绝新连接: 已有活跃连接存在。"); 
        return 1;  // 拒绝连接
    } 
    log_info("新 WebSocket 连接请求已授权，等待就绪..."); 
    return 0;  // 接受连接
}

// 处理连接关闭
void WebSocketServer::handle_websocket_close(const mg_connection* conn) { 
    const struct mg_request_info *ri = mg_get_request_info(conn); 
    log_info("WebSocket 连接正在关闭 (ID: " + std::to_string((uintptr_t)conn) + "): " + std::string(ri->request_uri)); 
    std::lock_guard<std::mutex> lock(connection_mutex_); 
    if (active_connection_ptr_ == conn) { 
        active_connection_ptr_ = nullptr; 
        log_info("活跃连接已清空。服务器现在可以接受新连接。"); 
    } 
}

// 处理WebSocket连接就绪
void WebSocketServer::handle_websocket_ready(mg_connection* conn) {
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        active_connection_ptr_ = conn;  // 设置为当前活跃连接
    }
    session_manager_.clearHistory();  // 清空会话历史
    log_info("WebSocket 连接已就绪，并已清空会话历史。");

    // 构造就绪消息
    nlohmann::json ready_msg = {
        {"type", "server_ready"},
        {"payload", {
            {"character_name", config_.get("Character", "CHARACTER_NAME", "AI")},
            {"character_identity", config_.get("Character", "CHARACTER_IDENTITY", "助手")},
            {"ui_config", {
                {"background_day", config_.get("UI", "BACKGROUND_DAY_PATH", "")},
                {"background_night", config_.get("UI", "BACKGROUND_NIGHT_PATH", "")},
                {"character_sprite_dir", config_.get("UI", "CHARACTER_SPRITE_DIR", "")},
                {"emotion_map", config_.getSection("EmotionMap")},  // 获取整个EmotionMap节
                {"sfx_dir", config_.get("UI", "SFX_DIR", "assets/sfx/")},
                {"emotion_sfx_map", config_.getSection("EmotionSfxMap")}  // 获取整个EmotionSfxMap节
            }}
        }}
    };
    
    // 发送就绪消息到前端
    send_websocket_message(conn, ready_msg.dump());
}

// 处理接收到的WebSocket数据
int WebSocketServer::handle_websocket_data(mg_connection* conn, int flags, char* data, size_t data_len) {
    // 只处理文本帧
    if (!(flags & MG_WEBSOCKET_OPCODE_TEXT)) { 
        return 1; 
    }
    
    std::string received_data(data, data_len);
    try {
        auto json_msg = nlohmann::json::parse(received_data);
        std::string msg_type = json_msg.value("type", "unknown");
        std::string prompt_for_ai;

        // 处理不同类型的消息
        if (msg_type == "user_message") {
            prompt_for_ai = json_msg["payload"].value("text", "");
        } 
        else if (msg_type == "system_command") {
            std::string command = json_msg["payload"].value("command", "");
            std::string value = json_msg["payload"].value("value", "");
            if(command == "set_time") prompt_for_ai = "{时间：" + value + "}";
            else if(command == "set_scene") prompt_for_ai = "{切换场景：" + value + "}";
        } 
        else {
            return 1;  // 忽略未知类型消息
        }
        
        if (prompt_for_ai.empty()) return 1;  // 忽略空消息

        // 添加用户消息到会话历史
        session_manager_.addMessage("user", prompt_for_ai);
        
        // 准备发送给AI的消息负载
        nlohmann::json messages_payload = nlohmann::json::array();
        
        // 加载系统提示词
        std::string system_prompt_str;
        const std::string prompt_file_path = config_.get("SystemPrompt", "PROMPT_FILE", "");
        if (!prompt_file_path.empty()) {
            std::ifstream file(prompt_file_path);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                system_prompt_str = buffer.str();
            } else {
                log_error("无法打开系统提示词文件: " + prompt_file_path);
            }
        }
        if (system_prompt_str.empty()) {
            log_warning("系统提示词为空，AI 可能无法按预期工作。");
        }
        
        // 添加系统提示词到消息负载
        messages_payload.push_back({{"role", "system"}, {"content", system_prompt_str}});
        
        // 添加历史消息到负载
        const nlohmann::json history = session_manager_.getHistoryAsJson();
        messages_payload.insert(messages_payload.end(), history.begin(), history.end());

        // 调用AI引擎生成响应
        log_info("正在调用 AI 引擎...");
        std::string ai_raw_response = engine_.generateResponse(messages_payload);
        log_info("AI 引擎返回: " + ai_raw_response);
        
        // 添加AI响应到会话历史
        session_manager_.addMessage("assistant", ai_raw_response);

        // 解析AI响应为结构化片段
        nlohmann::json segments = nlohmann::json::array();
        std::regex re_main("【(.+?)】(.+?)<(.+?)>");  // 主解析正则表达式
        
        // 使用迭代器处理所有匹配项
        auto sentences_begin = std::sregex_iterator(ai_raw_response.begin(), ai_raw_response.end(), re_main);
        auto sentences_end = std::sregex_iterator();

        for (std::sregex_iterator i = sentences_begin; i != sentences_end; ++i) {
            std::smatch match = *i;
            if (match.size() < 4) continue;  // 确保匹配有效

            // 提取各部分内容
            std::string expression = trim_string(match[1].str());  // 表情
            std::string middle_content = trim_string(match[2].str());  // 中间内容
            std::string text_jp = trim_string(match[3].str());  // 日语文本
            
            std::string action = "";
            std::string text_cn = "";  // 中文文本

            // 在中间内容中查找动作
            std::smatch action_match;
            std::regex re_action("\\((.+?)\\)");  // 动作解析正则
            if (std::regex_search(middle_content, action_match, re_action) && action_match.size() > 1) {
                action = trim_string(action_match[1].str());
                text_cn = trim_string(std::regex_replace(middle_content, re_action, ""));
            } else {
                text_cn = middle_content;
            }

            // 合成语音
            std::string audio_url = engine_.synthesizeSpeech(
                text_jp, 
                config_.get("API", "VOICE_API_URL", "")
            );

            // 构建响应片段
            segments.push_back({
                {"expression", expression},
                {"action", action},
                {"text_cn", text_cn},
                {"audio_url", audio_url}
            });
        }

        // 处理空响应情况
        if (segments.empty()) {
            log_warning("无法从AI响应中解析出格式化片段，将返回原始文本。");
            segments.push_back({
                {"expression", "default"}, 
                {"action", ""}, 
                {"text_cn", ai_raw_response}, 
                {"audio_url", ""}
            });
        }
        
        // 构建前端响应
        nlohmann::json response_to_frontend = {
            {"type", (msg_type == "system_command") ? "narration" : "ai_response"}, 
            {"payload", {{"segments", segments}}}
        };
        
        // 发送响应到前端
        send_websocket_message(conn, response_to_frontend.dump());

    } catch (const std::exception& e) {
        log_error("处理数据时发生错误: " + std::string(e.what()));
    }
    return 1;
}

// 发送WebSocket消息
void WebSocketServer::send_websocket_message(mg_connection* conn, const std::string& message) { 
    if (conn) { 
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, message.c_str(), message.length()); 
        log_info("发送 WebSocket 数据: " + message.substr(0, 200) + "...");  // 日志截断长消息
    } 
}

// 日志记录函数
void WebSocketServer::log_info(const std::string& message) const { 
    std::cout << "[信息] WebSocketServer: " << message << std::endl; 
}
void WebSocketServer::log_error(const std::string& message) const { 
    std::cerr << "[错误] WebSocketServer: " << message << std::endl; 
}
void WebSocketServer::log_warning(const std::string& message) const { 
    std::cerr << "[警告] WebSocketServer: " << message << std::endl; 
}