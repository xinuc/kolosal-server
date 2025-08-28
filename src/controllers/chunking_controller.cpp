#include "kolosal/controllers/chunking_controller.hpp"
#include "kolosal/models/chunking_request_model.hpp"
#include "kolosal/models/chunking_response_model.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <chrono>
#include <sstream>
#include <thread>

namespace kolosal {
namespace controllers {

ChunkingController::ChunkingController(NodeManager* node_manager)
    : node_manager_(node_manager)
    , chunking_service_(std::make_unique<retrieval::ChunkingService>())
    , request_counter_(0) {
    
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
    
    ServerLogger::logInfo("ChunkingController initialized");
}

ChunkingController::~ChunkingController() = default;

BaseController::Response ChunkingController::processChunking(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return processChunking(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing chunking: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ChunkingController::processChunking(const nlohmann::json& j) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Parse the request using the DTO model
        ChunkingRequest request;
        try {
            request.from_json(j);
        } catch (const std::runtime_error& ex) {
            return badRequest(ex.what());
        }
        
        // Validate the request
        if (!request.validate()) {
            return badRequest("Invalid request parameters");
        }
        
        // Validate or choose model only when embeddings are needed (semantic)
        if (request.method == "semantic") {
            std::string chosenModel = request.model_name;
            if (!validateChunkingModel(chosenModel)) {
                if (!node_manager_) {
                    return serverError("NodeManager not available");
                }
                
                // attempt to select any available model for embeddings as a fallback
                auto allModels = node_manager_->listEngineIds();
                bool found = false;
                for (const auto& id : allModels) {
                    auto eng = node_manager_->getEngine(id);
                    if (eng) {
                        chosenModel = id;
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    return notFound("No available embedding model found for semantic chunking", "model_name");
                }
            }
            // Use possibly updated model name
            request.model_name = chosenModel;
        }
        
        // Generate unique request ID
        std::string requestId = generateRequestId();
        
        ServerLogger::logInfo("Processing chunking request '%s' for model '%s' using method '%s'",
                              requestId.c_str(), request.model_name.c_str(), request.method.c_str());
        
        // Process the request based on the method
        std::future<std::vector<std::string>> chunks_future;
        
        if (request.method == "regular") {
            chunks_future = processRegularChunking(
                request.text,
                request.model_name,
                request.chunk_size,
                request.overlap
            );
        } else if (request.method == "semantic") {
            chunks_future = processSemanticChunking(
                request.text,
                request.model_name,
                request.chunk_size,
                request.overlap,
                request.max_chunk_size,
                request.similarity_threshold
            );
        } else {
            return badRequest("Invalid method: must be 'regular' or 'semantic'", "method");
        }
        
        // Wait for processing to complete
        std::vector<std::string> chunks;
        try {
            chunks = chunks_future.get();
        } catch (const std::exception& ex) {
            return serverError("Failed to process chunking request: " + std::string(ex.what()));
        }
        
        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        float processing_time_ms = static_cast<float>(duration.count());
        
        // Build response
        ChunkingResponse response;
        response.model_name = request.model_name;
        response.method = request.method;
        
        int original_tokens = estimateTokenCount(request.text);
        int total_chunk_tokens = 0;
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            int chunk_tokens = estimateTokenCount(chunks[i]);
            total_chunk_tokens += chunk_tokens;
            
            ChunkData chunk_data(chunks[i], static_cast<int>(i), chunk_tokens);
            response.addChunk(chunk_data);
        }
        
        response.setUsage(original_tokens, total_chunk_tokens, processing_time_ms);
        
        ServerLogger::logInfo("Successfully processed chunking request '%s': %zu chunks generated in %.2fms",
                              requestId.c_str(), chunks.size(), processing_time_ms);
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing chunking: %s", e.what());
        return serverError(e.what());
    }
}

std::future<std::vector<std::string>> ChunkingController::processRegularChunking(
    const std::string& text,
    const std::string& model_name,
    int chunk_size,
    int overlap
) {
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try {
            std::lock_guard<std::mutex> lock(service_mutex_);
            
            // Tokenize the text
            auto tokens_future = chunking_service_->tokenizeText(text, model_name);
            auto tokens = tokens_future.get();
            
            // Generate base chunks
            return chunking_service_->generateBaseChunks(text, tokens, chunk_size, overlap);
        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error in regular chunking: %s", 
                                   std::this_thread::get_id(), ex.what());
            throw;
        }
    });
}

std::future<std::vector<std::string>> ChunkingController::processSemanticChunking(
    const std::string& text,
    const std::string& model_name,
    int chunk_size,
    int overlap,
    int max_chunk_size,
    float similarity_threshold
) {
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try {
            std::lock_guard<std::mutex> lock(service_mutex_);
            
            // Use the semantic chunking service
            auto chunks_future = chunking_service_->semanticChunk(
                text, model_name, chunk_size, overlap, max_chunk_size, similarity_threshold
            );
            
            return chunks_future.get();
        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error in semantic chunking: %s", 
                                   std::this_thread::get_id(), ex.what());
            throw;
        }
    });
}

bool ChunkingController::validateChunkingModel(const std::string& model_name) const {
    if (!node_manager_) {
        return false;
    }
    
    auto engine = node_manager_->getEngine(model_name);
    return engine != nullptr;
}

int ChunkingController::estimateTokenCount(const std::string& text) const {
    // Simple estimation: roughly 4 characters per token for English text
    return std::max(1, static_cast<int>(text.length() / 4));
}

std::string ChunkingController::generateRequestId() {
    std::stringstream ss;
    ss << "chunk-" << ++request_counter_ << "-" 
       << std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();
    return ss.str();
}

} // namespace controllers
} // namespace kolosal