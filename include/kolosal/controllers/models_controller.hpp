#pragma once

#include "base_controller.hpp"
#include "../models/add_model_request_model.hpp"
#include "../models/add_model_response_model.hpp"
#include "../models/model_status_request_model.hpp"
#include "../models/model_status_response_model.hpp"
#include "../models/remove_model_request_model.hpp"
#include "../models/remove_model_response_model.hpp"
#include "../download_manager.hpp"
#include "inference_interface.h"
#include <string>
#include <vector>

namespace kolosal {

// Forward declarations
class NodeManager;

namespace controllers {

/**
 * Controller for model management operations
 * Handles listing, adding, removing, and status checking of AI models
 */
class ModelsController : public BaseController {
public:
    explicit ModelsController(NodeManager* node_manager = nullptr);
    
    /**
     * List all available models
     * @param openai_format If true, return in OpenAI API format
     */
    Response listModels(bool openai_format = false);
    
    /**
     * Add a new model
     * @param body JSON request body with model configuration
     */
    Response addModel(const std::string& body);
    Response addModel(const nlohmann::json& request);
    
    /**
     * Get specific model information
     * @param model_id The ID of the model to get
     */
    Response getModel(const std::string& model_id);
    
    /**
     * Remove a model
     * @param model_id The ID of the model to remove
     */
    Response removeModel(const std::string& model_id);
    
    /**
     * Get model status
     * @param model_id The ID of the model
     */
    Response getModelStatus(const std::string& model_id);
    
private:
    NodeManager* node_manager_;
    
    /**
     * Helper to get platform-specific default inference engine
     */
    std::string getPlatformDefaultInferenceEngine() const;
    
    /**
     * Helper to determine if a model is an embedding model
     */
    bool isEmbeddingModel(const std::string& model_id) const;
    
    /**
     * Helper to handle model download from URL
     */
    Response handleModelDownload(const std::string& model_id, 
                                const std::string& model_url,
                                const AddModelRequest& request);
    
    /**
     * Helper to validate model path
     */
    std::string validateModelPath(const std::string& path) const;
    
    /**
     * Helper to check and configure auto multi-GPU
     */
    void configureAutoMultiGPU(LoadingParameters& params) const;
};

} // namespace controllers
} // namespace kolosal