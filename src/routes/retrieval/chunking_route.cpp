#include "kolosal/routes/retrieval/chunking_route.hpp"
#include "kolosal/controllers/chunking_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{

ChunkingRoute::ChunkingRoute()
    : request_counter_(0)
{
    ServerLogger::logInfo("ChunkingRoute initialized");
}

ChunkingRoute::~ChunkingRoute() = default;

bool ChunkingRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" || method == "OPTIONS") && path == "/chunking")
    {
        current_method_ = method;
        return true;
    }
    return false;
}

void ChunkingRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for /chunking", 
                              std::this_thread::get_id(), current_method_.c_str());

        // Handle OPTIONS request for CORS preflight
        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
            return;
        }

        // Get NodeManager
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        
        // Create controller and process request
        controllers::ChunkingController controller(&nodeManager);
        auto response = controller.processChunking(body);
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling chunking request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}


void ChunkingRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for /chunking endpoint", 
                               std::this_thread::get_id());

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/plain"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
            {"Access-Control-Max-Age", "86400"} // Cache preflight for 24 hours
        };
        
        send_response(sock, 200, "", headers);
        
        ServerLogger::logDebug("[Thread %u] Successfully handled OPTIONS request", 
                               std::this_thread::get_id());
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling OPTIONS request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void ChunkingRoute::sendErrorResponse(
    SocketType sock,
    int status_code,
    const std::string& error_message,
    const std::string& error_type,
    const std::string& param
)
{
    json errorResponse;
    errorResponse["error"]["message"] = error_message;
    errorResponse["error"]["type"] = error_type;
    errorResponse["error"]["code"] = "";
    
    if (!param.empty())
    {
        errorResponse["error"]["param"] = param;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status_code, errorResponse.dump(), headers);
    
    ServerLogger::logError("[Thread %u] Chunking request error (%d): %s", 
                           std::this_thread::get_id(), status_code, error_message.c_str());
}


} // namespace kolosal
