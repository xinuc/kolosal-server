#pragma once

#include "base_controller.hpp"
#include <vector>
#include <future>
#include <string>

namespace kolosal {

// Forward declarations
class NodeManager;

namespace controllers {

/**
 * Controller for embedding generation operations
 * Follows SOLID principles:
 * - SRP: Only handles embedding business logic
 * - OCP: Extensible through virtual methods
 * - ISP: Focused interface for embedding operations
 * - DIP: Depends on abstractions (NodeManager interface)
 */
class EmbeddingController : public BaseController {
public:
    explicit EmbeddingController(NodeManager* node_manager = nullptr);
    
    /**
     * Generate embeddings for input text(s)
     * @param body JSON request body
     * @param openai_format If true, return in OpenAI-compatible format
     */
    Response generateEmbeddings(const std::string& body, bool openai_format = true);
    Response generateEmbeddings(const nlohmann::json& request, bool openai_format = true);
    
    /**
     * Handle OPTIONS request for CORS
     */
    Response handleOptions();
    
private:
    NodeManager* node_manager_;
    std::atomic<uint64_t> request_counter_{0};
    
    /**
     * Parse input from request (handles string or array)
     */
    std::vector<std::string> parseInputs(const nlohmann::json& request) const;
    
    /**
     * Process single embedding
     */
    std::vector<float> processSingleEmbedding(const std::string& model, const std::string& text) const;
    
    /**
     * Process embeddings in batch
     */
    std::vector<std::future<std::vector<float>>> processBatchEmbeddings(
        const std::string& model,
        const std::vector<std::string>& texts) const;
    
    /**
     * Estimate token count for text
     */
    int estimateTokenCount(const std::string& text) const;
    
    /**
     * Build response in OpenAI format
     */
    nlohmann::json buildOpenAIResponse(
        const std::string& model,
        const std::vector<std::vector<float>>& embeddings,
        int total_tokens) const;
    
    /**
     * Build response in standard format
     */
    nlohmann::json buildStandardResponse(
        const std::string& model,
        const std::vector<std::vector<float>>& embeddings,
        int total_tokens) const;
};

} // namespace controllers
} // namespace kolosal