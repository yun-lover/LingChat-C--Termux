#include "MemoryManager.hpp"
#include "Logger.hpp"
#include <fstream>
#include <cmath>
#include <numeric>
#include <algorithm>

// === 向量数学辅助函数 ===
// 计算两个向量的点积
float dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
}

// 计算向量的模长 (L2范数)
float magnitude(const std::vector<float>& v) {
    float sum_sq = 0.0f;
    for (float x : v) {
        sum_sq += x * x;
    }
    return std::sqrt(sum_sq);
}

// 计算两个向量的余弦相似度
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) {
        return 0.0f;
    }
    float mag_a = magnitude(a);
    float mag_b = magnitude(b);
    if (mag_a == 0.0f || mag_b == 0.0f) {
        return 0.0f;
    }
    return dot_product(a, b) / (mag_a * mag_b);
}
// =========================

MemoryManager::MemoryManager(const std::string& memory_file_path) : file_path_(memory_file_path) {
    std::ifstream file(file_path_);
    if (file.is_open()) {
        // 如果文件存在且不为空，则加载
        file >> memory_db_;
        Logger::logInfo("已从 " + file_path_ + " 加载 " + std::to_string(memory_db_.size()) + " 条记忆。");
    } else {
        // 如果文件不存在，初始化一个空的JSON数组
        memory_db_ = nlohmann::json::array();
        Logger::logInfo("未找到记忆文件，已初始化新的记忆库。");
    }
}

MemoryManager::~MemoryManager() {
    save_memories_to_file();
}

void MemoryManager::addMemory(const std::string& text_summary, const std::vector<float>& embedding) {
    // 将新记忆作为一个JSON对象添加到数组中
    nlohmann::json new_memory = {
        {"summary", text_summary},
        {"embedding", embedding}
    };
    memory_db_.push_back(new_memory);
    Logger::logInfo("已添加一条新记忆到内存中。当前总数: " + std::to_string(memory_db_.size()));
}

std::vector<std::string> MemoryManager::retrieveMemories(const std::vector<float>& query_embedding, int top_k) {
    if (memory_db_.empty()) {
        return {};
    }

    // 创建一个结构来存储相似度和对应的文本
    struct MemoryScore {
        float score;
        std::string summary;
    };
    std::vector<MemoryScore> scored_memories;

    // --- 核心的暴力搜索逻辑 ---
    for (const auto& memory_entry : memory_db_) {
        // 从JSON中提取向量
        std::vector<float> entry_embedding = memory_entry["embedding"].get<std::vector<float>>();
        
        // 计算相似度
        float score = cosine_similarity(query_embedding, entry_embedding);
        
        scored_memories.push_back({score, memory_entry["summary"]});
    }

    // 根据分数降序排序
    std::sort(scored_memories.begin(), scored_memories.end(), 
        [](const MemoryScore& a, const MemoryScore& b) {
            return a.score > b.score;
    });

    // 提取前 top_k 个结果的文本
    std::vector<std::string> top_memories;
    for (int i = 0; i < std::min((int)scored_memories.size(), top_k); ++i) {
        top_memories.push_back(scored_memories[i].summary);
    }
    
    Logger::logInfo("已检索到 " + std::to_string(top_memories.size()) + " 条最相关的记忆。");
    return top_memories;
}

void MemoryManager::save_memories_to_file() {
    std::ofstream file(file_path_);
    if (file.is_open()) {
        // 将内存中的JSON对象格式化后写入文件
        file << memory_db_.dump(4); // 使用4个空格进行缩进，方便阅读
        Logger::logInfo("已成功将 " + std::to_string(memory_db_.size()) + " 条记忆保存到 " + file_path_);
    } else {
        Logger::logError("无法打开文件 " + file_path_ + " 进行写入！");
    }
}