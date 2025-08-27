#include "kolosal/routes/retrieval/documents_route.hpp"
#include "kolosal/controllers/documents_controller.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{


DocumentsRoute::DocumentsRoute()
{
    ServerLogger::logInfo("DocumentsRoute initialized");
}

DocumentsRoute::~DocumentsRoute() = default;

bool DocumentsRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" && path == "/add_documents") ||
        (method == "POST" && path == "/remove_documents") ||
        (method == "GET" && path == "/list_documents") ||
        (method == "POST" && path == "/info_documents") ||
        (method == "POST" && path == "/retrieve") ||
        (method == "OPTIONS" && (path == "/add_documents" || path == "/remove_documents" || 
                                path == "/list_documents" || path == "/info_documents" || path == "/retrieve")))
    {
        current_endpoint_ = path;
        current_method_ = method;
        return true;
    }
    return false;
}

void DocumentsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for endpoint: %s", 
                              std::this_thread::get_id(), current_method_.c_str(), current_endpoint_.c_str());

        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
        }
        else if (current_endpoint_ == "/add_documents")
        {
            handleAddDocuments(sock, body);
        }
        else if (current_endpoint_ == "/remove_documents")
        {
            handleRemoveDocuments(sock, body);
        }
        else if (current_endpoint_ == "/list_documents")
        {
            handleListDocuments(sock);
        }
        else if (current_endpoint_ == "/info_documents")
        {
            handleDocumentsInfo(sock, body);
        }
        else if (current_endpoint_ == "/retrieve")
        {
            handleRetrieve(sock, body);
        }
        else
        {
            sendErrorResponse(sock, 404, "Endpoint not found");
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleAddDocuments(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received add documents request", std::this_thread::get_id());
        
        // Get database config
        auto& serverConfig = ServerConfig::getInstance();
        
        // Create controller and process request
        controllers::DocumentsController controller(&serverConfig.database);
        auto response = controller.addDocuments(body);
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling add documents request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleRemoveDocuments(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received remove documents request", std::this_thread::get_id());
        
        // Get database config
        auto& serverConfig = ServerConfig::getInstance();
        
        // Create controller and process request
        controllers::DocumentsController controller(&serverConfig.database);
        auto response = controller.removeDocuments(body);
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling remove documents request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleListDocuments(SocketType sock)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received list documents request", std::this_thread::get_id());
        
        // Get database config
        auto& serverConfig = ServerConfig::getInstance();
        
        // Create controller and process request
        controllers::DocumentsController controller(&serverConfig.database);
        auto response = controller.listDocuments();
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling list documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleDocumentsInfo(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received info documents request", std::this_thread::get_id());
        
        // Get database config
        auto& serverConfig = ServerConfig::getInstance();
        
        // Create controller and process request
        controllers::DocumentsController controller(&serverConfig.database);
        auto response = controller.getDocumentsInfo(body);
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling info documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for CORS preflight", 
                               std::this_thread::get_id());

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/plain"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
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

void DocumentsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                      const std::string& error_type, const std::string& param)
{
    kolosal::retrieval::DocumentsErrorResponse errorResponse;
    errorResponse.error = message;
    errorResponse.error_type = error_type;
    errorResponse.param = param;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, errorResponse.to_json().dump(), headers);
}


void DocumentsRoute::handleRetrieve(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received retrieve request", std::this_thread::get_id());
        
        // Get database config
        auto& serverConfig = ServerConfig::getInstance();
        
        // Create controller and process request
        controllers::DocumentsController controller(&serverConfig.database);
        auto response = controller.retrieveDocuments(body);
        
        // Add CORS headers
        std::map<std::string, std::string> headers = response.headers;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        
        send_response(sock, response.status_code, response.body.dump(), headers);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling retrieve request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

} // namespace kolosal
