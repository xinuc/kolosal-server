#include "kolosal/routes/llm/completion_route.hpp"
#include "kolosal/controllers/completion_controller.hpp"
#include "kolosal/controllers/chat_completion_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <map>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

CompletionRoute::CompletionRoute() = default;

CompletionRoute::~CompletionRoute() = default;

bool CompletionRoute::match(const std::string& method, const std::string& path) {
    return (method == "POST" && 
            (path == "/v1/inference/completions" || 
             path == "/inference/completions" ||
             path == "/v1/inference/chat/completions" || 
             path == "/inference/chat/completions"));
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
        
        // Check if streaming is requested
        bool isStreaming = j.value("streaming", false);
        
        // Determine which type of request (chat or text completion)
        bool isChatCompletion = j.contains("messages");
        bool isTextCompletion = j.contains("prompt");
        
        if (!isChatCompletion && !isTextCompletion) {
            json error = {
                {"error", {
                    {"message", "Invalid request: missing 'messages' or 'prompt' field"},
                    {"type", "invalid_request_error"}
                }}
            };
            send_response(sock, 400, error.dump());
            return;
        }
        
        // Extract model name for logging
        std::string modelName = j.value("model", "unknown");
        
        if (isStreaming) {
            // Handle streaming response
            ServerLogger::logInfo("[Thread %u] Processing streaming inference %s request for model '%s'",
                                  std::this_thread::get_id(), 
                                  isChatCompletion ? "chat completion" : "completion",
                                  modelName.c_str());
            
            // Start the streaming response with proper SSE headers
            begin_streaming_response(sock, 200, {
                {"Content-Type", "text/event-stream"},
                {"Cache-Control", "no-cache"},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "POST, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"}
            });
            
            bool streamCompleted = false;
            std::string lastError;
            
            // Create streaming callback
            auto streamingCallback = [sock, &streamCompleted](const std::string& data) {
                if (!data.empty()) {
                    // The controller already formats as SSE data
                    send_stream_chunk(sock, StreamChunk(data, false));
                }
            };
            
            // Process with streaming
            controllers::BaseController::Response response;
            if (isChatCompletion) {
                controllers::ChatCompletionController controller;
                response = controller.processChatCompletion(body, streamingCallback);
            } else {
                controllers::CompletionController controller;
                response = controller.processCompletion(body, streamingCallback);
            }
            
            // Check if there was an error during streaming
            if (response.status_code != 200) {
                // Send error as final chunk if not already sent
                json errorResponse;
                errorResponse["error"] = response.body.value("error", "Unknown error");
                errorResponse["text"] = "";
                errorResponse["tokens"] = json::array();
                errorResponse["tps"] = 0.0f;
                errorResponse["ttft"] = 0.0f;
                
                std::string sseData = "data: " + errorResponse.dump() + "\n\n";
                send_stream_chunk(sock, StreamChunk(sseData, false));
            }
            
            // Terminate the stream (empty chunk with terminate flag)
            send_stream_chunk(sock, StreamChunk("", true));
            
            ServerLogger::logInfo("[Thread %u] Completed streaming response for model '%s'",
                                  std::this_thread::get_id(), modelName.c_str());
            
        } else {
            // Handle non-streaming response
            ServerLogger::logInfo("[Thread %u] Processing non-streaming inference %s request for model '%s'",
                                  std::this_thread::get_id(),
                                  isChatCompletion ? "chat completion" : "completion",
                                  modelName.c_str());
            
            controllers::BaseController::Response response;
            
            if (isChatCompletion) {
                controllers::ChatCompletionController controller;
                response = controller.processChatCompletion(body);
            } else {
                controllers::CompletionController controller;
                response = controller.processCompletion(body);
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
            
            if (response.status_code == 200) {
                // Extract TPS for logging if available
                float tps = response.body.value("tps", 0.0f);
                ServerLogger::logInfo("[Thread %u] Completed non-streaming response for model '%s' (%.2f tokens/sec)",
                                      std::this_thread::get_id(), modelName.c_str(), tps);
            }
        }
        
    } catch (const json::parse_error& ex) {
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", 
                               std::this_thread::get_id(), ex.what());
        json error = {
            {"error", {
                {"message", std::string("Invalid JSON: ") + ex.what()},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::exception& ex) {
        ServerLogger::logError("[Thread %u] Error handling completion request: %s", 
                               std::this_thread::get_id(), ex.what());
        json error = {
            {"error", {
                {"message", std::string("Error: ") + ex.what()},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    }
}

} // namespace kolosal