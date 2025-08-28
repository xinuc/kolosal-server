#pragma once

#include "base_controller.hpp"
#include "inference_interface.h"
#include <functional>
#include <string>

namespace kolosal {

// Forward declarations
class NodeManager;

namespace controllers {

/**
 * Controller for text completion operations
 * Handles both standard completions and chat completions
 */
class CompletionController : public BaseController {
public:
    explicit CompletionController(NodeManager* node_manager = nullptr);
    
    /**
     * Handle text completion request
     * @param body JSON request body
     * @param streaming_callback Optional callback for streaming responses
     */
    Response processCompletion(const std::string& body,
                              std::function<void(const std::string&)> streaming_callback = nullptr);
    Response processCompletion(const nlohmann::json& request,
                              std::function<void(const std::string&)> streaming_callback = nullptr);
    
    /**
     * Handle chat completion request
     * @param body JSON request body
     * @param streaming_callback Optional callback for streaming responses
     */
    Response processChatCompletion(const std::string& body,
                                  std::function<void(const std::string&)> streaming_callback = nullptr);
    Response processChatCompletion(const nlohmann::json& request,
                                  std::function<void(const std::string&)> streaming_callback = nullptr);
    
    /**
     * Determine if request is for chat completion based on content
     */
    bool isChatCompletionRequest(const nlohmann::json& request) const;
    
private:
    NodeManager* node_manager_;
    
    /**
     * Parse completion parameters from JSON
     */
    CompletionParameters parseCompletionParameters(const nlohmann::json& j) const;
    
    /**
     * Parse chat completion parameters from JSON
     */
    ChatCompletionParameters parseChatCompletionParameters(const nlohmann::json& j) const;
    
    /**
     * Convert completion result to JSON
     */
    nlohmann::json completionResultToJson(const CompletionResult& result) const;
    
    /**
     * Finalize structured output settings (grammar vs jsonSchema)
     */
    template<typename P>
    void finalizeStructuredOutput(P& params, const char* context) const;
    
    /**
     * Handle streaming completion
     */
    void handleStreamingCompletion(IInferenceEngine* engine, int jobId,
                                  std::function<void(const std::string&)> callback) const;
};

} // namespace controllers
} // namespace kolosal