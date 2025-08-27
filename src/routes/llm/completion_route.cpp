#include "kolosal/routes/llm/completion_route.hpp"
#include "kolosal/controllers/completion_controller.hpp"
#include "kolosal/controllers/chat_completion_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <map>

using json = nlohmann::json;

namespace kolosal {

CompletionRoute::CompletionRoute() = default;

CompletionRoute::~CompletionRoute() = default;

bool CompletionRoute::match(const std::string& method, const std::string& path) {
    return (method == "POST" && 
            (path == "/v1/completions" || 
             path == "/completions" || 
             path == "/v1/chat/completions" || 
             path == "/chat/completions"));
}

void CompletionRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Check for empty body
        if (body.empty()) {
            json error = {
                {"error", {
                    {"message", "Request body is empty"},
                    {"type", "invalid_request_error"}
                }}
            };
            send_response(sock, 400, error.dump());
            return;
        }

        auto j = json::parse(body);
        
        // Determine which controller to use based on request content
        controllers::BaseController::Response response;
        
        if (j.contains("messages")) {
            // Chat completion request
            controllers::ChatCompletionController controller;
            response = controller.processChatCompletion(body); // nullptr callback = no streaming
        } else if (j.contains("prompt")) {
            // Text completion request
            controllers::CompletionController controller;
            response = controller.processCompletion(body); // nullptr callback = no streaming
        } else {
            json error = {
                {"error", {
                    {"message", "Invalid request: missing 'messages' or 'prompt' field"},
                    {"type", "invalid_request_error"}
                }}
            };
            send_response(sock, 400, error.dump());
            return;
        }
        
        // Add CORS headers
        std::map<std::string, std::string> cors_headers = {
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"}
        };
        
        // Merge response headers with CORS headers
        for (const auto& [key, value] : response.headers) {
            cors_headers[key] = value;
        }
        
        // Send the response
        send_response(sock, response.status_code, response.body.dump(), cors_headers);
        
    } catch (const json::parse_error& ex) {
        ServerLogger::logError("JSON parsing error: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Invalid JSON: ") + ex.what()},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error handling completion request: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Internal server error: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

} // namespace kolosal