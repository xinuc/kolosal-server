#include "kolosal/routes/retrieval/embedding_route.hpp"
#include "kolosal/controllers/embedding_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <map>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

EmbeddingRoute::EmbeddingRoute() : request_counter_(0) {
    ServerLogger::logInfo("EmbeddingRoute initialized");
}

EmbeddingRoute::~EmbeddingRoute() = default;

bool EmbeddingRoute::match(const std::string& method, const std::string& path) {
    return (method == "POST" && (path == "/v1/embeddings" || path == "/embeddings"));
}

void EmbeddingRoute::handle(SocketType sock, const std::string& body) {
    try {
        ServerLogger::logInfo("[Thread %u] Received embedding request", std::this_thread::get_id());
        
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
        
        // Use the controller to handle the request
        controllers::EmbeddingController controller;
        auto response = controller.generateEmbeddings(body, true); // OpenAI format
        
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
        
        ServerLogger::logInfo("[Thread %u] Successfully sent embedding response", 
                            std::this_thread::get_id());
        
    } catch (const json::parse_error& ex) {
        ServerLogger::logError("JSON parsing error in embedding route: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Invalid JSON: ") + ex.what()},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::invalid_argument& ex) {
        ServerLogger::logError("Invalid argument in embedding route: %s", ex.what());
        json error = {
            {"error", {
                {"message", ex.what()},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::runtime_error& ex) {
        ServerLogger::logError("Runtime error in embedding route: %s", ex.what());
        json error = {
            {"error", {
                {"message", ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error handling embedding request: %s", ex.what());
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