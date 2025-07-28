#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // <-- 我们唯一的“数据库”

class MemoryManager {
public:
    /**
     * @brief 构造函数，从一个JSON文件加载或创建记忆库。
     * @param memory_file_path 用于存储记忆的JSON文件路径。
     */
    explicit MemoryManager(const std::string& memory_file_path);
    ~MemoryManager();

    /**
     * @brief 将一条新的记忆存入内存，并将在程序结束时保存到文件。
     * @param text_summary 记忆的文本内容。
     * @param embedding 记忆的向量表示。
     */
    void addMemory(const std::string& text_summary, const std::vector<float>& embedding);

    /**
     * @brief 根据查询向量，检索最相关的K条记忆。
     * @param query_embedding 用于查询的向量。
     * @param top_k 需要检索的记忆数量。
     * @return 一个包含相关记忆文本的向量。
     */
    std::vector<std::string> retrieveMemories(const std::vector<float>& query_embedding, int top_k);

private:
    std::string file_path_;
    nlohmann::json memory_db_; // 在内存中持有一个JSON对象作为我们的数据库

    // 辅助函数，用于将内存中的DB写入文件
    void save_memories_to_file();
};

#endif // MEMORY_MANAGER_HPP