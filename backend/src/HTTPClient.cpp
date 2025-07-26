#include "HTTPClient.hpp"
#include <stdexcept>
#include <iostream>

HTTPClient::HTTPClient(const std::string& api_key) : api_key_(api_key) {
    static bool curl_global_init_done = false;
    if (!curl_global_init_done) {
        CURLcode res = curl_global_init(CURL_GLOBAL_ALL & ~CURL_GLOBAL_SSL);
        if (res != CURLE_OK) {
            throw std::runtime_error("curl_global_init() failed: " + std::string(curl_easy_strerror(res)));
        }
        curl_global_init_done = true;
    }

    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("无法初始化 cURL 句柄。");
    }
    std::cout << "[信息] HTTPClient 已初始化 (无SSL初始化)。" << std::endl;
}

HTTPClient::~HTTPClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

size_t HTTPClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string HTTPClient::post(const std::string& url, const std::string& data, const std::vector<std::string>& headers) {
    return sendRequest(url, "POST", data, headers);
}

std::string HTTPClient::get(const std::string& url, const std::vector<std::string>& headers) {
    return sendRequest(url, "GET", "", headers);
}

std::string HTTPClient::sendRequest(const std::string& url, const std::string& method, const std::string& data, const std::vector<std::string>& additional_headers) {
    if (!curl_) throw std::runtime_error("cURL 句柄无效。");
    curl_easy_reset(curl_);
    std::string response_string;
    struct curl_slist* chunk = nullptr;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
    if (method == "POST") {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.length());
    } else if (method != "GET") {
        throw std::runtime_error("不支持的 HTTP 方法: " + method);
    }
    chunk = curl_slist_append(chunk, "Content-Type: application/json");
    if (!api_key_.empty()) {
        std::string auth_header = "Authorization: Bearer " + api_key_;
        chunk = curl_slist_append(chunk, auth_header.c_str());
    }
    for (const auto& header : additional_headers) {
        chunk = curl_slist_append(chunk, header.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(chunk);
    if (res != CURLE_OK) {
        throw std::runtime_error("cURL 请求失败: " + std::string(curl_easy_strerror(res)));
    }
    return response_string;
}