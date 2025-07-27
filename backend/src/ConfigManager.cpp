#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

// 辅助函数：去除字符串首尾的空白字符
std::string trim(const std::string& s) {
    const char* ws = " \t\n\r\f\v";
    size_t first = s.find_first_not_of(ws);
    if (std::string::npos == first) return "";
    size_t last = s.find_last_not_of(ws);
    return s.substr(first, (last - first + 1));
}

ConfigManager::ConfigManager(const std::string& env_file_name) {
    std::vector<std::string> paths_to_try;
    paths_to_try.push_back(env_file_name);

    const char* home_env = getenv("HOME");
    if (home_env) {
        paths_to_try.push_back(std::string(home_env) + "/.config/chat/.env");
    }

    bool loaded = false;
    for (const auto& path : paths_to_try) {
        if (fs::exists(path)) {
            parseFile(path);
            loaded_config_file_path_ = path;
            log_info("已成功加载并解析配置文件: " + path);
            loaded = true;
            break; 
        }
    }

    if (!loaded) {
        log_warning("在所有预设路径中均未找到有效的配置文件。");
    }
}

void ConfigManager::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty()) continue;

        // 使用 line.front() 而不是 line == '['
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.length() - 2));
            continue;
        }

        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiter_pos));
            std::string value = trim(line.substr(delimiter_pos + 1));
            
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            if (!key.empty()) {
                config_data_[current_section][key] = value;
            }
        }
    }
}

std::string ConfigManager::get(const std::string& section, const std::string& key, const std::string& default_value) const {
    // 允许在没有section的情况下获取值（兼容旧的直接key=value格式）
    if (config_data_.count(section)) {
        auto const& sec = config_data_.at(section);
        if (sec.count(key)) {
            return sec.at(key);
        }
    }
    // 增加对 "default" section 的回退查找
    if (config_data_.count("default")) {
        auto const& def_sec = config_data_.at("default");
        if(def_sec.count(key)) {
            return def_sec.at(key);
        }
    }
    return default_value;
}

std::map<std::string, std::string> ConfigManager::getSection(const std::string& section) const {
    auto it = config_data_.find(section);
    if (it != config_data_.end()) {
        return it->second;
    }
    return {};
}

std::string ConfigManager::getLoadedConfigFile() const { return loaded_config_file_path_; }
void ConfigManager::log_info(const std::string& message) const { std::cout << "[信息] ConfigManager: " << message << std::endl; }
void ConfigManager::log_warning(const std::string& message) const { std::cerr << "[警告] ConfigManager: " << message << std::endl; }