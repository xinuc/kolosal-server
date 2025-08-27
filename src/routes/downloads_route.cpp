#include "kolosal/routes/downloads_route.hpp"
#include "kolosal/controllers/downloads_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <thread>
#include <map>

using json = nlohmann::json;

namespace kolosal {

bool DownloadsRoute::match(const std::string& method, const std::string& path) {
    // Match various download endpoints:
    // GET /downloads or /v1/downloads (all downloads status)
    // GET /downloads/{model_id} or /v1/downloads/{model_id} (specific download progress)
    // DELETE /downloads/{model_id} or /v1/downloads/{model_id} (cancel download)
    // POST /downloads/{model_id}/cancel or /v1/downloads/{model_id}/cancel (cancel download)
    // POST /downloads/{model_id}/pause or /v1/downloads/{model_id}/pause (pause download)
    // POST /downloads/{model_id}/resume or /v1/downloads/{model_id}/resume (resume download)
    // DELETE /downloads or /v1/downloads (cancel all downloads)
    // POST /downloads/cancel or /v1/downloads/cancel (cancel all downloads)
    
    std::regex all_pattern(R"(^(?:/v1)?/downloads$)");
    std::regex single_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
    std::regex cancel_pattern(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
    std::regex pause_pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
    std::regex resume_pattern(R"(^(?:/v1)?/downloads/([^/]+)/resume$)");
    std::regex cancel_all_pattern(R"(^(?:/v1)?/downloads/cancel$)");
    
    bool matches = false;
    
    if (method == "GET") {
        matches = std::regex_match(path, all_pattern) || std::regex_match(path, single_pattern);
    } else if (method == "DELETE") {
        matches = std::regex_match(path, all_pattern) || std::regex_match(path, single_pattern);
    } else if (method == "POST") {
        matches = std::regex_match(path, cancel_pattern) || 
                 std::regex_match(path, pause_pattern) ||
                 std::regex_match(path, resume_pattern) ||
                 std::regex_match(path, cancel_all_pattern) ||
                 std::regex_match(path, all_pattern); // POST to /downloads for cancel all
    }
    
    if (matches) {
        // Store the path for later use in handle method
        matched_path_ = path;
    }
    
    return matches;
}

void DownloadsRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Create the controller instance
        controllers::DownloadsController controller;
        controllers::BaseController::Response response;
        
        // Determine the action based on path patterns
        std::regex all_pattern(R"(^(?:/v1)?/downloads$)");
        std::regex single_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
        std::regex cancel_pattern(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
        std::regex pause_pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
        std::regex resume_pattern(R"(^(?:/v1)?/downloads/([^/]+)/resume$)");
        std::regex cancel_all_pattern(R"(^(?:/v1)?/downloads/cancel$)");
        
        std::smatch match;
        
        // Handle cancel single download
        if (std::regex_match(matched_path_, match, cancel_pattern)) {
            if (match.size() > 1) {
                std::string model_id = match[1].str();
                response = controller.cancelDownload(model_id);
            } else {
                json jError = {
                    {"error", {
                        {"message", "Cannot extract model ID from cancel request path"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "path"}, 
                        {"code", "invalid_path_format"}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }
        }
        // Handle pause download
        else if (std::regex_match(matched_path_, match, pause_pattern)) {
            if (match.size() > 1) {
                std::string model_id = match[1].str();
                response = controller.pauseDownload(model_id);
            } else {
                json jError = {
                    {"error", {
                        {"message", "Cannot extract model ID from pause request path"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "path"}, 
                        {"code", "invalid_path_format"}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }
        }
        // Handle resume download
        else if (std::regex_match(matched_path_, match, resume_pattern)) {
            if (match.size() > 1) {
                std::string model_id = match[1].str();
                response = controller.resumeDownload(model_id);
            } else {
                json jError = {
                    {"error", {
                        {"message", "Cannot extract model ID from resume request path"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "path"}, 
                        {"code", "invalid_path_format"}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }
        }
        // Handle cancel all downloads
        else if (std::regex_match(matched_path_, cancel_all_pattern) || 
                (std::regex_match(matched_path_, all_pattern) && body.find("cancel") != std::string::npos)) {
            response = controller.cancelAllDownloads();
        }
        // Handle all downloads status
        else if (std::regex_match(matched_path_, all_pattern)) {
            response = controller.getAllDownloads();
        }
        // Handle specific download progress or cancel
        else if (std::regex_match(matched_path_, match, single_pattern)) {
            if (match.size() > 1) {
                std::string model_id = match[1].str();
                // This could be either GET (progress) or DELETE (cancel)
                // For now, we'll assume GET for progress since we need to determine HTTP method
                // In a real implementation, you'd check the actual HTTP method
                response = controller.getDownloadProgress(model_id);
            } else {
                json jError = {
                    {"error", {
                        {"message", "Cannot extract model ID from request path. Please ensure the URL format is /downloads/{model-id}"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "path"}, 
                        {"code", "invalid_path_format"}
                    }}
                };
                send_response(sock, 400, jError.dump());
                ServerLogger::logError("[Thread %u] Cannot extract model ID from request path: %s", 
                                     std::this_thread::get_id(), matched_path_.c_str());
                return;
            }
        } else {
            json jError = {
                {"error", {
                    {"message", "Invalid downloads endpoint"}, 
                    {"type", "invalid_request_error"}, 
                    {"param", "path"}, 
                    {"code", "invalid_endpoint"}
                }}
            };
            send_response(sock, 400, jError.dump());
            return;
        }
        
        // Add CORS headers to the response
        std::map<std::string, std::string> cors_headers = {
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"}
        };
        
        // Send the response
        send_response(sock, response.status_code, response.body.dump(), cors_headers);
        
    } catch (const std::exception& ex) {
        ServerLogger::logError("[Thread %u] Error handling downloads request: %s", 
                             std::this_thread::get_id(), ex.what());
        
        json jError = {
            {"error", {
                {"message", std::string("Server error: ") + ex.what()}, 
                {"type", "server_error"}, 
                {"param", nullptr}, 
                {"code", nullptr}
            }}
        };
        
        send_response(sock, 500, jError.dump());
    }
}

} // namespace kolosal