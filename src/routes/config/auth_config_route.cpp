#include "kolosal/routes/config/auth_config_route.hpp"
#include "kolosal/controllers/auth_config_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <map>

using json = nlohmann::json;

namespace kolosal {

AuthConfigRoute::AuthConfigRoute() {
    ServerLogger::logInfo("Auth config route initialized");
}

bool AuthConfigRoute::match(const std::string& method, const std::string& path) {
    return (path.find("/v1/auth") == 0) &&
           ((method == "GET" && (path == "/v1/auth/config" || path == "/v1/auth/stats")) ||
            (method == "PUT" && path == "/v1/auth/config") ||
            (method == "POST" && path == "/v1/auth/clear"));
}

void AuthConfigRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Parse the request to determine the action
        controllers::AuthConfigController controller;
        controllers::BaseController::Response response;
        
        // Determine action based on request pattern
        // In real implementation, this would be determined from HTTP method and path
        if (body.empty()) {
            // GET request - default to config
            response = controller.getConfig();
        } else {
            json j = json::parse(body);
            
            if (j.contains("action")) {
                std::string action = j["action"];
                if (action == "get_config") {
                    response = controller.getConfig();
                } else if (action == "update_config") {
                    response = controller.updateConfig(body);
                } else if (action == "get_stats") {
                    response = controller.getStats();
                } else if (action == "clear_rate_limit") {
                    response = controller.clearRateLimit(body);
                } else {
                    json error = {
                        {"error", {
                            {"message", "Unknown action: " + action},
                            {"type", "invalid_request_error"}
                        }}
                    };
                    send_response(sock, 400, error.dump());
                    return;
                }
            } else {
                // Assume it's a config update if no action specified
                response = controller.updateConfig(body);
            }
        }
        
        // Add CORS headers to the response
        std::map<std::string, std::string> cors_headers = {
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"}
        };
        
        // Send the response
        send_response(sock, response.status_code, response.body.dump(), cors_headers);
        
    } catch (const json::exception& ex) {
        ServerLogger::logError("JSON parsing error in auth config route: %s", ex.what());
        json error = {
            {"error", {
                {"message", "Invalid JSON in request body"},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error in auth config route: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Internal error: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

} // namespace kolosal