#pragma once

#include "base_controller.hpp"

namespace kolosal {

// Forward declarations
class NodeManager;

namespace controllers {

/**
 * Controller for health status operations
 * Handles health checks and server status reporting
 */
class HealthController : public BaseController {
public:
    explicit HealthController(NodeManager* node_manager = nullptr);
    
    /**
     * Get health status of the server
     * Returns server status, engine information, and metrics
     */
    Response getHealthStatus();
    
    /**
     * Handle OPTIONS request for CORS
     */
    Response handleOptions();
    
private:
    NodeManager* node_manager_;
};

} // namespace controllers
} // namespace kolosal