#include "kolosal/routes/engines_route.hpp"
#include "kolosal/controllers/engines_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"

namespace kolosal
{

    bool EnginesRoute::match(const std::string &method, const std::string &path)
    {
        bool matches = ((method == "GET" || method == "POST" || method == "PUT") && (path == "/engines" || path == "/v1/engines"));
        
        // Store matched method for use in handle()
        if (matches)
        {
            matched_method_ = method;
        }
        
        return matches;
    }

    void EnginesRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Dependency Injection - Get dependencies
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            
            // Single Responsibility - Controller handles business logic
            controllers::EnginesController controller(&nodeManager);
            
            // Route based on HTTP method
            controllers::BaseController::Response response;
            
            if (matched_method_ == "GET")
            {
                response = controller.listEngines();
            }
            else if (matched_method_ == "POST")
            {
                response = controller.addEngine(body);
            }
            else if (matched_method_ == "PUT")
            {
                response = controller.setDefaultEngine(body);
            }
            else
            {
                sendErrorResponse(sock, 405, 
                    "Method not allowed. Use GET to list engines, POST to add engines, or PUT to set default engine.",
                    "method_not_allowed");
                return;
            }
            
            // Merge CORS headers with response headers
            auto headers = mergeHeaders(getCorsHeaders(), response.headers);
            send_response(sock, response.status_code, response.body.dump(), headers);
        }
        catch (const std::exception &ex)
        {
            logError(std::string("Error handling engines request: ") + ex.what());
            sendErrorResponse(sock, 500, std::string("Server error: ") + ex.what());
        }
    }


} // namespace kolosal
