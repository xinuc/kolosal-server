#include "kolosal/controllers/embedding_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <future>
#include <chrono>

namespace kolosal {
namespace controllers {

EmbeddingController::EmbeddingController(NodeManager* node_manager)
    : node_manager_(node_manager) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response EmbeddingController::generateEmbeddings(const std::string& body, bool openai_format) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return generateEmbeddings(json, openai_format);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error generating embeddings: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EmbeddingController::generateEmbeddings(const nlohmann::json& request, bool openai_format) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Extract model
        if (!request.contains("model") || !request["model"].is_string()) {
            return badRequest("Missing or invalid 'model' field", "model");
        }
        std::string model = request["model"];
        
        // Parse inputs
        auto inputs = parseInputs(request);
        if (inputs.empty()) {
            return badRequest("No input provided", "input");
        }
        
        // Check if model exists
        auto engine = node_manager_->getEngine(model);
        if (!engine) {
            return notFound("Model '" + model + "' not found or could not be loaded", "model");
        }
        
        ServerLogger::logInfo("Processing %zu embedding request(s) for model '%s'", 
                            inputs.size(), model.c_str());
        
        // Process embeddings
        std::vector<std::vector<float>> embeddings;
        int totalTokens = 0;
        
        if (inputs.size() > 1) {
            // Batch processing
            auto futures = processBatchEmbeddings(model, inputs);
            for (auto& future : futures) {
                embeddings.push_back(future.get());
            }
        } else {
            // Single embedding
            embeddings.push_back(processSingleEmbedding(model, inputs[0]));
        }
        
        // Calculate token usage
        for (const auto& input : inputs) {
            totalTokens += estimateTokenCount(input);
        }
        
        // Build response
        nlohmann::json response;
        if (openai_format) {
            response = buildOpenAIResponse(model, embeddings, totalTokens);
        } else {
            response = buildStandardResponse(model, embeddings, totalTokens);
        }
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error generating embeddings: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response EmbeddingController::handleOptions() {
    Response response(200);
    response.headers["Content-Type"] = "text/plain";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
    response.headers["Access-Control-Max-Age"] = "86400";
    response.body = "";
    return response;
}

std::vector<std::string> EmbeddingController::parseInputs(const nlohmann::json& request) const {
    std::vector<std::string> inputs;
    
    if (request.contains("input")) {
        const auto& input = request["input"];
        
        if (input.is_string()) {
            inputs.push_back(input.get<std::string>());
        } else if (input.is_array()) {
            for (const auto& item : input) {
                if (item.is_string()) {
                    inputs.push_back(item.get<std::string>());
                }
            }
        }
    }
    
    return inputs;
}

std::vector<float> EmbeddingController::processSingleEmbedding(const std::string& model, const std::string& text) const {
    auto engine = node_manager_->getEngine(model);
    if (!engine) {
        throw std::runtime_error("Model not available");
    }
    
    // Create embedding parameters
    EmbeddingParameters params;
    params.input = text;
    params.normalize = true; // Default normalization
    params.seqId = static_cast<int>(std::hash<std::string>{}(text + model) % 10000);
    
    if (!params.isValid()) {
        throw std::runtime_error("Invalid embedding parameters");
    }
    
    // Submit embedding job
    int jobId = engine->submitEmbeddingJob(params);
    if (jobId < 0) {
        throw std::runtime_error("Failed to submit embedding job");
    }
    
    // Wait for job completion
    engine->waitForJob(jobId);
    
    // Check for errors
    if (engine->hasJobError(jobId)) {
        std::string error = engine->getJobError(jobId);
        throw std::runtime_error("Embedding error: " + error);
    }
    
    // Get the embedding result
    EmbeddingResult result = engine->getEmbeddingResult(jobId);
    return result.embedding;
}

std::vector<std::future<std::vector<float>>> EmbeddingController::processBatchEmbeddings(
    const std::string& model,
    const std::vector<std::string>& texts) const {
    
    std::vector<std::future<std::vector<float>>> futures;
    
    for (const auto& text : texts) {
        futures.push_back(std::async(std::launch::async, 
            [this, model, text]() {
                return processSingleEmbedding(model, text);
            }));
    }
    
    return futures;
}

int EmbeddingController::estimateTokenCount(const std::string& text) const {
    // Simple estimation: ~4 characters per token
    return std::max(1, static_cast<int>(text.length() / 4));
}

nlohmann::json EmbeddingController::buildOpenAIResponse(
    const std::string& model,
    const std::vector<std::vector<float>>& embeddings,
    int total_tokens) const {
    
    nlohmann::json response = {
        {"object", "list"},
        {"data", nlohmann::json::array()},
        {"model", model},
        {"usage", {
            {"prompt_tokens", total_tokens},
            {"total_tokens", total_tokens}
        }}
    };
    
    for (size_t i = 0; i < embeddings.size(); ++i) {
        response["data"].push_back({
            {"object", "embedding"},
            {"embedding", embeddings[i]},
            {"index", i}
        });
    }
    
    return response;
}

nlohmann::json EmbeddingController::buildStandardResponse(
    const std::string& model,
    const std::vector<std::vector<float>>& embeddings,
    int total_tokens) const {
    
    return {
        {"model", model},
        {"embeddings", embeddings},
        {"usage", {
            {"prompt_tokens", total_tokens},
            {"total_tokens", total_tokens}
        }},
        {"count", embeddings.size()},
        {"dimension", embeddings.empty() ? 0 : embeddings[0].size()}
    };
}

} // namespace controllers
} // namespace kolosal