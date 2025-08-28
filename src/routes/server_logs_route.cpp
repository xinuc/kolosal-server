#include "kolosal/routes/server_logs_route.hpp"
#include "kolosal/controllers/server_logs_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{

    bool ServerLogsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/logs" || path == "/v1/logs" || path == "/server/logs"));
    }

    void ServerLogsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received server logs request", std::this_thread::get_id());

            // Create controller and get logs
            controllers::ServerLogsController controller;
            auto response = controller.getLogs();
            
            // Add CORS headers
            std::map<std::string, std::string> headers = response.headers;
            headers["Access-Control-Allow-Origin"] = "*";
            headers["Access-Control-Allow-Methods"] = "GET, OPTIONS";
            headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
            
            send_response(sock, response.status_code, response.body.dump(), headers);
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling server logs request: %s", std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {
                    {"message", std::string("Server error: ") + ex.what()}, 
                    {"type", "server_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };

            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"},
                {"Access-Control-Allow-Origin", "*"}
            };
            
            send_response(sock, 500, jError.dump(), headers);
        }
    }

} // namespace kolosal
