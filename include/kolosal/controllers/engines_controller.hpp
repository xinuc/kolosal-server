#pragma once

#include "base_controller.hpp"
#include <string>

namespace kolosal {

// Forward declarations
class NodeManager;
class ServerConfig;

namespace controllers {

/**
 * Controller for inference engine management
 * Follows SOLID principles:
 * - SRP: Only handles engine management business logic
 * - OCP: Extensible for new engine types
 * - DIP: Depends on abstractions
 */
class EnginesController : public BaseController {
public:
    explicit EnginesController(NodeManager* node_manager = nullptr);
    
    /**
     * List available inference engines
     */
    Response listEngines();
    
    /**
     * Add a new inference engine
     * @param body JSON request body with engine details
     */
    Response addEngine(const std::string& body);
    Response addEngine(const nlohmann::json& request);
    
    /**
     * Set the default inference engine
     * @param body JSON request body with engine name
     */
    Response setDefaultEngine(const std::string& body);
    Response setDefaultEngine(const nlohmann::json& request);
    
private:
    NodeManager* node_manager_;
    ServerConfig* config_;
    
    /**
     * Validate engine configuration
     */
    bool validateEngineConfig(const nlohmann::json& config) const;
    
    /**
     * Check if library path exists
     */
    bool validateLibraryPath(const std::string& path) const;
};

} // namespace controllers
} // namespace kolosal