#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/controllers/health_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"

namespace kolosal
{

    bool HealthStatusRoute::match(const std::string &method, const std::string &path)
    {
        if ((method == "GET" || method == "OPTIONS") && 
            (path == "/health" || path == "/v1/health" || path == "/status"))
        {
            current_method_ = method;
            return true;
        }
        return false;
    }

    void HealthStatusRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Dependency Injection - Get dependencies
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            
            // Single Responsibility - Controller handles business logic
            controllers::HealthController controller(&nodeManager);
            
            // Route only handles HTTP concerns
            controllers::BaseController::Response response;
            
            if (current_method_ == "OPTIONS")
            {
                response = controller.handleOptions();
            }
            else
            {
                response = controller.getHealthStatus();
            }
            
            // Merge CORS headers with response headers  
            auto headers = mergeHeaders(getCorsHeaders(), response.headers);
            send_response(sock, response.status_code, response.body.dump(), headers);
        }
        catch (const std::exception &ex)
        {
            logError(std::string("Error handling health request: ") + ex.what());
            sendErrorResponse(sock, 500, std::string("Server error: ") + ex.what());
        }
    }

} // namespace kolosal
