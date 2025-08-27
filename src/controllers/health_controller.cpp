#include "kolosal/controllers/health_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <chrono>

namespace kolosal {
namespace controllers {

HealthController::HealthController(NodeManager* node_manager)
    : node_manager_(node_manager) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response HealthController::getHealthStatus() {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Collect engine metrics
        auto engineIds = node_manager_->listEngineIds();
        
        int loadedCount = 0;
        int unloadedCount = 0;
        nlohmann::json engineSummary = nlohmann::json::array();
        
        for (const auto& engineId : engineIds) {
            // Check engine status without loading it
            auto [exists, isLoaded] = node_manager_->getEngineStatus(engineId);
            if (isLoaded) {
                loadedCount++;
            } else {
                unloadedCount++;
            }
            
            engineSummary.push_back({
                {"engine_id", engineId},
                {"status", isLoaded ? "loaded" : "unloaded"}
            });
        }
        
        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        nlohmann::json response = {
            {"status", "healthy"},
            {"timestamp", millis},
            {"server", {
                {"name", "Kolosal Inference Server"},
                {"version", "1.0.0"},
                {"uptime", "running"}
            }},
            {"node_manager", {
                {"total_engines", engineIds.size()},
                {"loaded_engines", loadedCount},
                {"unloaded_engines", unloadedCount},
                {"autoscaling", "enabled"}
            }},
            {"engines", engineSummary}
        };
        
        ServerLogger::logDebug("Health status check - %zu engines total (%d loaded, %d unloaded)",
                              engineIds.size(), loadedCount, unloadedCount);
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting health status: %s", e.what());
        return serverError("Server error: " + std::string(e.what()));
    }
}

BaseController::Response HealthController::handleOptions() {
    Response response(200);
    response.headers["Content-Type"] = "text/plain";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
    response.headers["Access-Control-Max-Age"] = "86400";
    response.body = "";
    return response;
}

} // namespace controllers
} // namespace kolosal