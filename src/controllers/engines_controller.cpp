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

BaseController::Response EnginesController::addEngine(const nlohmann::json& requestData) {
    try {
        ServerLogger::logDebug("Received add inference engine request");
        
        // Validate required fields
        if (!requestData.contains("name") || !requestData.contains("library_path")) {
            nlohmann::json error = {
                {"error", {
                    {"message", "Missing required fields: 'name' and 'library_path' are required"},
                    {"type", "invalid_request_error"},
                    {"param", "body"},
                    {"code", nullptr}
                }}
            };
            return Response(400, error);
        }
        
        // Extract engine configuration
        std::string engineName = requestData["name"];
        std::string libraryPath = requestData["library_path"];
        std::string description = requestData.value("description", "");
        bool loadOnStartup = requestData.value("load_on_startup", true);
        
        // Validate engine name and path uniqueness
        auto& config = ServerConfig::getInstance();
        for (const auto& existingEngine : config.inferenceEngines) {
            if (existingEngine.name == engineName && existingEngine.library_path == libraryPath) {
                // Engine with same name and path already exists
                // Check actual loading status from NodeManager
                if (!node_manager_) {
                    node_manager_ = &ServerAPI::instance().getNodeManager();
                }
                auto availableEngines = node_manager_->getAvailableInferenceEngines();
                
                bool actuallyLoaded = false;
                for (const auto& engine : availableEngines) {
                    if (engine.name == engineName && engine.library_path == libraryPath) {
                        actuallyLoaded = engine.is_loaded;
                        break;
                    }
                }
                
                nlohmann::json response = {
                    {"message", "Engine with name '" + engineName + "' and path '" + libraryPath + "' already exists"},
                    {"status", "success"},
                    {"engine", {
                        {"name", existingEngine.name},
                        {"library_path", existingEngine.library_path},
                        {"description", existingEngine.description},
                        {"load_on_startup", existingEngine.load_on_startup},
                        {"is_loaded", actuallyLoaded}
                    }}
                };
                ServerLogger::logInfo("Engine '%s' already exists in config - actual load status: %s",
                                    engineName.c_str(), actuallyLoaded ? "loaded" : "not loaded");
                return Response(200, response);
            } else if (existingEngine.name == engineName) {
                nlohmann::json error = {
                    {"error", {
                        {"message", "Engine with name '" + engineName + "' already exists with different path"},
                        {"type", "invalid_request_error"},
                        {"param", "name"},
                        {"code", nullptr}
                    }}
                };
                return Response(409, error);
            }
        }
        
        // Validate library path exists
        if (!std::filesystem::exists(libraryPath)) {
            nlohmann::json error = {
                {"error", {
                    {"message", "Library file not found: " + libraryPath},
                    {"type", "invalid_request_error"},
                    {"param", "library_path"},
                    {"code", nullptr}
                }}
            };
            return Response(400, error);
        }
        
        // Create new engine configuration
        InferenceEngineConfig newEngine(engineName, ServerConfig::makeAbsolutePath(libraryPath), description);
        newEngine.load_on_startup = loadOnStartup;
        
        // Add to server config
        config.inferenceEngines.push_back(newEngine);
        
        // Save updated config to file
        ServerLogger::logInfo("About to save configuration after adding engine '%s'", engineName.c_str());
        ServerLogger::logInfo("Current config file path in EnginesController: '%s'", config.getCurrentConfigFilePath().c_str());
        
        if (!config.saveToCurrentFile()) {
            // Remove the engine from memory if save failed
            config.inferenceEngines.pop_back();
            
            nlohmann::json error = {
                {"error", {
                    {"message", "Failed to save configuration to file"},
                    {"type", "server_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }}
            };
            return Response(500, error);
        }
        
        // Reconfigure inference loader with updated engines
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        if (!node_manager_->reconfigureEngines(config.inferenceEngines)) {
            ServerLogger::logWarning("Failed to reconfigure inference engines after adding new engine");
        }
        
        // Prepare response
        nlohmann::json engineInfo = {
            {"name", newEngine.name},
            {"library_path", newEngine.library_path},
            {"description", newEngine.description},
            {"load_on_startup", newEngine.load_on_startup},
            {"is_loaded", false} // Newly added engines are not loaded by default
        };
        
        nlohmann::json response = {
            {"message", "Inference engine added successfully"},
            {"engine", engineInfo}
        };
        
        ServerLogger::logInfo("Successfully added inference engine: %s", engineName.c_str());
        return Response(201, response);
        
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error handling add inference engine request: %s", ex.what());
        
        nlohmann::json error = {
            {"error", {
                {"message", std::string("Server error: ") + ex.what()},
                {"type", "server_error"},
                {"param", nullptr},
                {"code", nullptr}
            }}
        };
        return Response(500, error);
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

BaseController::Response EnginesController::setDefaultEngine(const nlohmann::json& requestData) {
    try {
        ServerLogger::logDebug("Received set default inference engine request");
        
        // Validate required field
        if (!requestData.contains("engine_name")) {
            nlohmann::json error = {
                {"error", {
                    {"message", "Missing required field: 'engine_name' is required"},
                    {"type", "invalid_request_error"},
                    {"param", "body"},
                    {"code", nullptr}
                }}
            };
            return Response(400, error);
        }
        
        std::string engineName = requestData["engine_name"];
        
        // Validate that the engine exists in the configuration
        auto& config = ServerConfig::getInstance();
        bool engineExists = false;
        for (const auto& engine : config.inferenceEngines) {
            if (engine.name == engineName) {
                engineExists = true;
                break;
            }
        }
        
        if (!engineExists) {
            nlohmann::json error = {
                {"error", {
                    {"message", "Engine '" + engineName + "' not found in configuration"},
                    {"type", "invalid_request_error"},
                    {"param", "engine_name"},
                    {"code", nullptr}
                }}
            };
            return Response(404, error);
        }
        
        // Update the default engine in config
        config.defaultInferenceEngine = engineName;
        
        // Save updated config to file
        ServerLogger::logInfo("About to save configuration after setting default engine to '%s'", engineName.c_str());
        ServerLogger::logInfo("Current config file path in EnginesController: '%s'", config.getCurrentConfigFilePath().c_str());
        
        if (!config.saveToCurrentFile()) {
            nlohmann::json error = {
                {"error", {
                    {"message", "Failed to save configuration to file"},
                    {"type", "server_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }}
            };
            return Response(500, error);
        }
        
        // Prepare response
        nlohmann::json response = {
            {"message", "Default inference engine set successfully"},
            {"default_engine", engineName}
        };
        
        ServerLogger::logInfo("Successfully set default inference engine to: %s", engineName.c_str());
        return Response(200, response);
        
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error handling set default inference engine request: %s", ex.what());
        
        nlohmann::json error = {
            {"error", {
                {"message", std::string("Server error: ") + ex.what()},
                {"type", "server_error"},
                {"param", nullptr},
                {"code", nullptr}
            }}
        };
        return Response(500, error);
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