#include "kolosal/routes/retrieval/internet_search_route.hpp"
#include "kolosal/controllers/internet_search_controller.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;

namespace kolosal {


    InternetSearchRoute::InternetSearchRoute(const SearchConfig& config) : config_(config) {
        ServerLogger::logInfo("InternetSearchRoute initialized (enabled: %s)", 
                              config_.enabled ? "true" : "false");
    }

    InternetSearchRoute::~InternetSearchRoute() = default;


    bool InternetSearchRoute::match(const std::string& method, const std::string& path) {
        return (method == "GET" || method == "POST") && 
               (path == "/internet_search" || path == "/v1/internet_search" || path == "/search");
    }


    void InternetSearchRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received internet search request", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
            // Create controller with config
            controllers::SearchConfig searchConfig;
            searchConfig.enabled = config_.enabled;
            searchConfig.searxng_url = config_.searxng_url;
            searchConfig.api_key = config_.api_key;
            searchConfig.timeout = config_.timeout;
            searchConfig.max_results = config_.max_results;
            searchConfig.default_engine = config_.default_engine;
            searchConfig.default_category = config_.default_category;
            searchConfig.default_language = config_.default_language;
            searchConfig.default_format = config_.default_format;
            
            controllers::InternetSearchController controller(searchConfig);
            auto response = controller.search(body);
            
            // Add CORS headers
            std::map<std::string, std::string> headers = response.headers;
            headers["Access-Control-Allow-Origin"] = "*";
            headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
            headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
            
            send_response(sock, response.status_code, response.body.dump(), headers);
            
            ServerLogger::logInfo("[Thread %u] Search request completed", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling search request: %s", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()), ex.what());
            
            json error_response = {
                {"error", {
                    {"type", "internal_error"},
                    {"message", "Internal server error"}
                }}
            };
            
            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"},
                {"Access-Control-Allow-Origin", "*"}
            };
            
            send_response(sock, 500, error_response.dump(), headers);
        }
    }

} // namespace kolosal
