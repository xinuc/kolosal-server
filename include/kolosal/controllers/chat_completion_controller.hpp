#pragma once

#include "base_controller.hpp"
#include "inference_interface.h"
#include <functional>
#include <string>
#include <variant>

namespace kolosal {

// Forward declarations
class NodeManager;

namespace controllers {

/**
 * Controller for OpenAI-compatible chat completions
 * Follows SOLID principles:
 * - SRP: Only handles OpenAI chat completion logic
 * - OCP: Extensible for new features (tools, functions)
 * - ISP: Focused interface for chat completions
 * - DIP: Depends on abstractions
 */
class ChatCompletionController : public BaseController {
public:
    explicit ChatCompletionController(NodeManager* node_manager = nullptr);
    
    /**
     * Process OpenAI-compatible chat completion request
     * @param body JSON request body
     * @param streaming_callback Optional callback for streaming responses
     */
    Response processChatCompletion(const std::string& body,
                                  std::function<void(const std::string&)> streaming_callback = nullptr);
    Response processChatCompletion(const nlohmann::json& request,
                                  std::function<void(const std::string&)> streaming_callback = nullptr);
    
    /**
     * Process OpenAI-compatible text completion request
     * @param body JSON request body
     * @param streaming_callback Optional callback for streaming responses
     */
    Response processCompletion(const std::string& body,
                             std::function<void(const std::string&)> streaming_callback = nullptr);
    Response processCompletion(const nlohmann::json& request,
                             std::function<void(const std::string&)> streaming_callback = nullptr);
    
private:
    NodeManager* node_manager_;
    
    /**
     * Parse OpenAI chat parameters to internal format
     */
    ChatCompletionParameters parseOpenAIChatParameters(const nlohmann::json& request) const;
    
    /**
     * Parse OpenAI completion parameters to internal format
     */
    CompletionParameters parseOpenAICompletionParameters(const nlohmann::json& request) const;
    
    /**
     * Build OpenAI-compatible chat response
     */
    nlohmann::json buildOpenAIChatResponse(
        const CompletionResult& result,
        const std::string& model,
        const std::string& id,
        bool isStream = false) const;
    
    /**
     * Build OpenAI-compatible completion response
     */
    nlohmann::json buildOpenAICompletionResponse(
        const CompletionResult& result,
        const std::string& model,
        const std::string& id,
        bool isStream = false) const;
    
    /**
     * Generate unique completion ID
     */
    std::string generateCompletionId() const;
    
    /**
     * Handle streaming for OpenAI format
     */
    void handleOpenAIStreaming(
        IInferenceEngine* engine,
        int jobId,
        const std::string& model,
        const std::string& id,
        bool isChat,
        std::function<void(const std::string&)> callback) const;
    
    /**
     * Handle response_format for structured output
     */
    void applyResponseFormat(ChatCompletionParameters& params, const nlohmann::json& request) const;
    void applyResponseFormat(CompletionParameters& params, const nlohmann::json& request) const;
};

} // namespace controllers
} // namespace kolosal