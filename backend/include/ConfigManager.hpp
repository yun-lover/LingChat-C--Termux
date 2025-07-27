#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>

class ConfigManager {
public:
    ConfigManager(const std::string& env_file_name = ".env");
    
    std::string get(const std::string& section,
                 const std::string& key, 
                 const std::string& default_value = "") const;
    std::map<std::string, std::string> getSection(const std::string& section) const;
    std::string getLoadedConfigFile() const;

private:
    std::map<std::string, std::map<std::string, 
    std::string>> config_data_;
    std::string loaded_config_file_path_;

    void parseFile(const std::string& filename); 
    
    void log_info(const std::string& message) const;
    void log_warning(const std::string& message) const;
};

#endif // CONFIG_MANAGER_HPP