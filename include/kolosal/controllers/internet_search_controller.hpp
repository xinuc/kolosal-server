#ifndef KOLOSAL_CONTROLLERS_INTERNET_SEARCH_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_INTERNET_SEARCH_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"
#include <string>
#include <map>
#include <future>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

namespace kolosal {
namespace controllers {

struct SearchConfig {
    bool enabled = false;
    std::string searxng_url = "http://localhost:8888";
    std::string api_key = "";
    int timeout = 30;
    int max_results = 10;
    std::string default_engine = "";
    std::string default_category = "general";
    std::string default_language = "en";
    std::string default_format = "json";
};

struct SearchRequest {
    std::string query;
    std::string engines;
    std::string categories;
    std::string language;
    std::string format;
    int results = 10;
    bool safe_search = true;
    int timeout = 30;
};

struct SearchResult {
    bool success = false;
    std::string response_body;
    std::string error_message;
    int status_code = 0;
};

class InternetSearchController : public BaseController {
public:
    explicit InternetSearchController(const SearchConfig& config = SearchConfig());
    ~InternetSearchController();

    Response search(const std::string& body);
    Response search(const nlohmann::json& request);
    
    // Check if search is enabled
    bool isEnabled() const { return config_.enabled; }

private:
    struct HttpSearchRequest {
        std::string url;
        std::map<std::string, std::string> headers;
        int timeout;
        std::shared_ptr<std::promise<SearchResult>> promise;
    };

    // Configuration
    SearchConfig config_;
    
    // Worker thread management
    std::vector<std::thread> worker_threads_;
    std::queue<std::shared_ptr<HttpSearchRequest>> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    
    // Helper methods
    void startWorkerThreads();
    void stopWorkerThreads();
    void workerLoop();
    
    SearchRequest parseSearchRequest(const nlohmann::json& json);
    std::string validateRequest(const SearchRequest& request);
    std::string buildSearchUrl(const SearchRequest& request);
    std::future<SearchResult> makeHttpRequest(const std::string& url, 
                                             const std::map<std::string, std::string>& headers,
                                             int timeout);
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static std::string urlEncode(const std::string& str);
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_INTERNET_SEARCH_CONTROLLER_HPP