#include "kolosal/controllers/engines_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <filesystem>

namespace kolosal {
namespace controllers {

EnginesController::EnginesController(NodeManager* node_manager)
    : node_manager_(node_manager), config_(&ServerConfig::getInstance()) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response EnginesController::listEngines() {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Get available engines
        auto availableEngines = node_manager_->getAvailableInferenceEngines();
        
        // Get default engine from config
        std::string defaultEngine = config_->defaultInferenceEngine;
        
        nlohmann::json enginesList = nlohmann::json::array();
        for (const auto& engine : availableEngines) {
            enginesList.push_back({
                {"name", engine.name},
                {"version", engine.version},
                {"description", engine.description},
                {"library_path", engine.library_path},
                {"is_loaded", engine.is_loaded},
                {"is_default", engine.name == defaultEngine}
            });
        }
        
        nlohmann::json response = {
            {"inference_engines", enginesList},
            {"default_engine", defaultEngine},
            {"total_count", enginesList.size()}
        };
        
        ServerLogger::logDebug("Listed %zu inference engines", enginesList.size());
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing engines: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EnginesController::addEngine(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is required");
        }
        
        auto json = parseJsonBody(body);
        return addEngine(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error adding engine: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EnginesController::addEngine(const nlohmann::json& request) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Validate request
        if (!validateEngineConfig(request)) {
            return badRequest("Invalid engine configuration. Required fields: name, library_path");
        }
        
        std::string name = request["name"];
        std::string libraryPath = request["library_path"];
        
        // Validate library path
        if (!validateLibraryPath(libraryPath)) {
            return badRequest("Library path does not exist: " + libraryPath, "library_path");
        }
        
        // Extract optional fields
        std::string version = request.value("version", "1.0");
        std::string description = request.value("description", "");
        
        // Add engine to NodeManager
        // Note: NodeManager doesn't have a direct registerInferenceEngine method
        // This would need to be implemented in NodeManager
        bool success = false;
        try {
            // For now, we can only report available engines, not add new ones
            // This would require modifying NodeManager API
            return serverError("Adding new inference engines at runtime is not yet supported");
        } catch (...) {
            success = false;
        }
        
        if (success) {
            nlohmann::json response = {
                {"message", "Engine added successfully"},
                {"engine", {
                    {"name", name},
                    {"library_path", libraryPath},
                    {"version", version}
                }}
            };
            
            ServerLogger::logInfo("Added inference engine: %s", name.c_str());
            return Response(201, response);  // Created
            
        } else {
            return serverError("Failed to add engine. It may already exist or the library is invalid.");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error adding engine: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EnginesController::setDefaultEngine(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is required");
        }
        
        auto json = parseJsonBody(body);
        return setDefaultEngine(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error setting default engine: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EnginesController::setDefaultEngine(const nlohmann::json& request) {
    try {
        if (!request.contains("engine") || !request["engine"].is_string()) {
            return badRequest("Missing or invalid 'engine' field", "engine");
        }
        
        std::string engineName = request["engine"];
        
        // Verify engine exists
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        auto engines = node_manager_->getAvailableInferenceEngines();
        bool engineExists = false;
        
        for (const auto& engine : engines) {
            if (engine.name == engineName) {
                engineExists = true;
                break;
            }
        }
        
        if (!engineExists) {
            return notFound("Engine not found: " + engineName, "engine");
        }
        
        // Update config
        config_->defaultInferenceEngine = engineName;
        
        // Update config in memory (saveConfig not available)
        nlohmann::json response = {
            {"message", "Default engine updated successfully"},
            {"default_engine", engineName}
        };
        
        ServerLogger::logInfo("Set default engine to: %s", engineName.c_str());
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error setting default engine: %s", e.what());
        return serverError(e.what());
    }
}

bool EnginesController::validateEngineConfig(const nlohmann::json& config) const {
    return config.contains("name") && config["name"].is_string() &&
           config.contains("library_path") && config["library_path"].is_string();
}

bool EnginesController::validateLibraryPath(const std::string& path) const {
    try {
        return std::filesystem::exists(path);
    } catch (...) {
        return false;
    }
}

} // namespace controllers
} // namespace kolosal