#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <string>
#include <vector>
#include <curl/curl.h>

class HTTPClient {
public:
    explicit HTTPClient(const std::string& api_key = "");
    ~HTTPClient();

    HTTPClient(const HTTPClient&) = delete;
    HTTPClient& operator=(const HTTPClient&) = delete;
    HTTPClient(HTTPClient&&) = delete;
    HTTPClient& operator=(HTTPClient&&) = delete;

    std::string post(const std::string& url, 
                  const std::string& data, 
                  const std::vector<std::string>& headers = {});
    std::string get(const std::string& url, 
                 const std::vector<std::string>& headers = {});

private:
    std::string sendRequest(const std::string& url, 
                          const std::string& method, 
                          const std::string& data, 
                          const std::vector<std::string>& additional_headers);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    CURL* curl_;
    std::string api_key_;
};

#endif // HTTP_CLIENT_HPP