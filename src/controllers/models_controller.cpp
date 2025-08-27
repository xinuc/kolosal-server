#include "kolosal/controllers/models_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/download_utils.hpp"
#include "llama.h"
#include <algorithm>
#include <filesystem>

namespace kolosal {
namespace controllers {

ModelsController::ModelsController(NodeManager* node_manager)
    : node_manager_(node_manager) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response ModelsController::listModels(bool openai_format) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        auto engineIds = node_manager_->listEngineIds();
        
        nlohmann::json modelsList = nlohmann::json::array();
        int embeddingModels = 0;
        int llmModels = 0;
        int loadedModels = 0;
        int unloadedModels = 0;
        
        for (const auto& engineId : engineIds) {
            auto [exists, isLoaded] = node_manager_->getEngineStatus(engineId);
            
            nlohmann::json modelInfo = {
                {"model_id", engineId},
                {"status", isLoaded ? "loaded" : "unloaded"},
                {"available", exists},
                {"last_accessed", "recently"}
            };
            
            // Determine model type
            if (isEmbeddingModel(engineId)) {
                modelInfo["model_type"] = "embedding";
                modelInfo["capabilities"] = nlohmann::json::array({"embedding", "retrieval"});
                embeddingModels++;
            } else {
                modelInfo["model_type"] = "llm";
                modelInfo["capabilities"] = nlohmann::json::array({"text_generation", "chat"});
                llmModels++;
            }
            
            if (isLoaded) {
                loadedModels++;
                modelInfo["inference_ready"] = true;
            } else {
                unloadedModels++;
                modelInfo["inference_ready"] = false;
            }
            
            modelsList.push_back(modelInfo);
        }
        
        nlohmann::json response;
        
        if (openai_format) {
            // OpenAI-compatible format
            response = {
                {"object", "list"},
                {"data", modelsList}
            };
        } else {
            // Our standard format with summary
            response = {
                {"models", modelsList},
                {"total_count", modelsList.size()},
                {"summary", {
                    {"total_models", modelsList.size()},
                    {"embedding_models", embeddingModels},
                    {"llm_models", llmModels},
                    {"loaded_models", loadedModels},
                    {"unloaded_models", unloadedModels}
                }}
            };
        }
        
        ServerLogger::logDebug("Listed %zu models (%d embedding, %d LLM, %d loaded)",
                              modelsList.size(), embeddingModels, llmModels, loadedModels);
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing models: %s", e.what());
        return serverError("Server error: " + std::string(e.what()));
    }
}

BaseController::Response ModelsController::addModel(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is required", "body");
        }
        
        auto json = parseJsonBody(body);
        return addModel(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what(), "body");
    } catch (const std::exception& e) {
        ServerLogger::logError("Error adding model: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ModelsController::addModel(const nlohmann::json& request_json) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Parse request
        AddModelRequest request;
        request.from_json(request_json);
        
        if (!request.validate()) {
            return badRequest("Invalid request parameters");
        }
        
        std::string modelId = request.model_id;
        std::string modelPath = request.model_path;
        std::string modelType = request.model_type;
        
        // Validate model type
        if (modelType != "llm" && modelType != "embedding") {
            return badRequest("Invalid model_type. Must be 'llm' or 'embedding'", "model_type");
        }
        
        // Use default inference engine if not specified
        std::string inferenceEngine = request.inference_engine;
        if (inferenceEngine.empty()) {
            auto& config = ServerConfig::getInstance();
            inferenceEngine = config.defaultInferenceEngine.empty() ? 
                getPlatformDefaultInferenceEngine() : config.defaultInferenceEngine;
        }
        
        // Check if model path is URL
        if (is_valid_url(modelPath)) {
            return handleModelDownload(modelId, modelPath, request);
        }
        
        // Validate local path
        std::string actualModelPath = validateModelPath(modelPath);
        if (actualModelPath.empty()) {
            return badRequest("Model path '" + modelPath + "' does not exist or is invalid", "model_path");
        }
        
        // Convert loading parameters
        LoadingParameters loadParams;
        loadParams.n_ctx = request.loading_parameters.n_ctx;
        loadParams.n_batch = request.loading_parameters.n_batch;
        loadParams.n_gpu_layers = request.loading_parameters.n_gpu_layers;
        loadParams.use_mmap = request.loading_parameters.use_mmap;
        loadParams.cont_batching = request.loading_parameters.cont_batching;
        // ... copy other parameters as needed
        
        // Configure auto multi-GPU if needed
        configureAutoMultiGPU(loadParams);
        
        // Add or register the engine
        bool success = false;
        if (request.load_immediately) {
            if (modelType == "embedding") {
                success = node_manager_->addEmbeddingEngine(modelId, actualModelPath.c_str(), 
                                                           loadParams, request.main_gpu_id);
            } else {
                success = node_manager_->addEngine(modelId, actualModelPath.c_str(), 
                                                  loadParams, request.main_gpu_id, inferenceEngine);
            }
        } else {
            if (modelType == "embedding") {
                success = node_manager_->registerEmbeddingEngine(modelId, actualModelPath.c_str(),
                                                                loadParams, request.main_gpu_id);
            } else {
                success = node_manager_->registerEngine(modelId, actualModelPath.c_str(),
                                                       loadParams, request.main_gpu_id);
            }
        }
        
        if (success) {
            nlohmann::json response = {
                {"model_id", modelId},
                {"model_path", modelPath},
                {"model_type", modelType},
                {"status", request.load_immediately ? "loaded" : "registered"},
                {"load_immediately", request.load_immediately},
                {"message", "Model added successfully"}
            };
            
            ServerLogger::logInfo("Successfully added model '%s'", modelId.c_str());
            return Response(201, response);  // Created
            
        } else {
            // Check if model already exists
            auto engineIds = node_manager_->listEngineIds();
            if (std::find(engineIds.begin(), engineIds.end(), modelId) != engineIds.end()) {
                return conflict("Model ID '" + modelId + "' already exists");
            }
            return serverError("Failed to add model '" + modelId + "'");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error adding model: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ModelsController::getModel(const std::string& model_id) {
    return getModelStatus(model_id);  // Same implementation for now
}

BaseController::Response ModelsController::removeModel(const std::string& model_id) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        bool success = node_manager_->removeEngine(model_id);
        
        if (success) {
            nlohmann::json response = {
                {"model_id", model_id},
                {"status", "removed"},
                {"message", "Model removed successfully"}
            };
            
            ServerLogger::logInfo("Successfully removed model '%s'", model_id.c_str());
            return ok(response);
            
        } else {
            return notFound("Model not found or could not be removed", "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error removing model: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ModelsController::getModelStatus(const std::string& model_id) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        auto engineIds = node_manager_->listEngineIds();
        
        if (std::find(engineIds.begin(), engineIds.end(), model_id) != engineIds.end()) {
            auto [exists, isLoaded] = node_manager_->getEngineStatus(model_id);
            
            nlohmann::json response = {
                {"model_id", model_id},
                {"status", isLoaded ? "loaded" : "unloaded"},
                {"available", true},
                {"message", isLoaded ? "Model is loaded and ready" : "Model exists but is currently unloaded"},
                {"engine_loaded", isLoaded},
                {"inference_ready", isLoaded}
            };
            
            if (isLoaded) {
                response["capabilities"] = nlohmann::json::array({"inference"});
            } else {
                response["capabilities"] = nlohmann::json::array();
            }
            
            ServerLogger::logInfo("Retrieved status for model '%s'", model_id.c_str());
            return ok(response);
            
        } else {
            return notFound("Model not found", "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting model status: %s", e.what());
        return serverError(e.what());
    }
}

std::string ModelsController::getPlatformDefaultInferenceEngine() const {
#ifdef __APPLE__
    return "ggml";
#else
    return "llamacpp";
#endif
}

bool ModelsController::isEmbeddingModel(const std::string& model_id) const {
    std::string lower = model_id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return (lower.find("embedding") != std::string::npos ||
            lower.find("embed") != std::string::npos ||
            lower.find("retrieval") != std::string::npos);
}

BaseController::Response ModelsController::handleModelDownload(
    const std::string& model_id,
    const std::string& model_url,
    const AddModelRequest& request) {
    
    // Generate download path
    std::string downloadPath = generate_download_path_executable(model_url);
    
    // Check if already downloaded
    if (std::filesystem::exists(downloadPath)) {
        auto& download_manager = DownloadManager::getInstance();
        auto progress = download_manager.getDownloadProgress(model_id);
        
        if (progress && progress->total_bytes > 0) {
            std::uintmax_t fileSize = std::filesystem::file_size(downloadPath);
            if (fileSize >= progress->total_bytes) {
                // File is complete, proceed with normal add
                AddModelRequest localRequest = request;
                localRequest.model_path = downloadPath;
                return addModel(localRequest.to_json());
            }
        }
    }
    
    // Start download
    auto& download_manager = DownloadManager::getInstance();
    
    EngineCreationParams engine_params;
    engine_params.model_id = model_id;
    engine_params.model_type = request.model_type;
    engine_params.load_immediately = request.load_immediately;
    engine_params.main_gpu_id = request.main_gpu_id;
    // Convert loading parameters manually
    LoadingParameters loadParams;
    loadParams.n_ctx = request.loading_parameters.n_ctx;
    loadParams.n_batch = request.loading_parameters.n_batch;
    loadParams.n_gpu_layers = request.loading_parameters.n_gpu_layers;
    loadParams.use_mmap = request.loading_parameters.use_mmap;
    loadParams.cont_batching = request.loading_parameters.cont_batching;
    engine_params.loading_params = loadParams;
    engine_params.inference_engine = request.inference_engine;
    
    bool started = download_manager.startDownloadWithEngine(
        model_id, model_url, downloadPath, engine_params);
    
    if (started) {
        nlohmann::json response = {
            {"model_id", model_id},
            {"model_type", request.model_type},
            {"status", "downloading"},
            {"message", "Download started in background"},
            {"download_url", model_url},
            {"local_path", downloadPath}
        };
        
        return Response(202, response);  // Accepted
        
    } else if (download_manager.isDownloadInProgress(model_id)) {
        nlohmann::json response = {
            {"message", "Model download already in progress"},
            {"model_id", model_id},
            {"status", "downloading"}
        };
        
        return Response(202, response);  // Accepted
        
    } else {
        return serverError("Failed to start download");
    }
}

std::string ModelsController::validateModelPath(const std::string& path) const {
    if (!std::filesystem::exists(path)) {
        return "";
    }
    
    if (std::filesystem::is_directory(path)) {
        // Look for .gguf file in directory
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                return entry.path().string();
            }
        }
        return "";
    }
    
    if (std::filesystem::is_regular_file(path)) {
        if (std::filesystem::path(path).extension() == ".gguf") {
            return path;
        }
    }
    
    return "";
}

void ModelsController::configureAutoMultiGPU(LoadingParameters& params) const {
    try {
        bool wants_auto = (params.tensor_split.empty() && 
                          params.n_gpu_layers > 0 && 
                          params.split_mode <= 0);
        
        if (wants_auto) {
            size_t dev_count = llama_max_devices();
            if (dev_count > 1) {
                params.split_mode = 1;  // layer split
                params.tensor_split.assign(dev_count, 1.0f / static_cast<float>(dev_count));
                
                // Fix floating point drift
                float sum = 0.0f;
                for (size_t i = 0; i < dev_count - 1; ++i) {
                    sum += params.tensor_split[i];
                }
                params.tensor_split.back() = 1.0f - sum;
                
                ServerLogger::logInfo("Auto multi-GPU enabled: %zu devices", dev_count);
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logWarning("Auto multi-GPU setup failed: %s", e.what());
    }
}

} // namespace controllers
} // namespace kolosal