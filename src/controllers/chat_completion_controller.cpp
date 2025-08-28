#include "kolosal/controllers/chat_completion_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <chrono>
#include <sstream>
#include <random>

namespace kolosal {
namespace controllers {

ChatCompletionController::ChatCompletionController(NodeManager* node_manager)
    : node_manager_(node_manager) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response ChatCompletionController::processChatCompletion(
    const std::string& body,
    std::function<void(const std::string&)> streaming_callback) {
    
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return processChatCompletion(json, streaming_callback);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing chat completion: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ChatCompletionController::processChatCompletion(
    const nlohmann::json& request,
    std::function<void(const std::string&)> streaming_callback) {
    
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Extract model
        if (!request.contains("model") || !request["model"].is_string()) {
            return badRequest("Missing or invalid 'model' field", "model");
        }
        std::string model = request["model"];
        
        // Parse parameters
        ChatCompletionParameters params = parseOpenAIChatParameters(request);
        
        // Apply response_format if present
        applyResponseFormat(params, request);
        
        if (!params.isValid()) {
            return badRequest("Invalid chat completion parameters");
        }
        
        // Get engine
        auto engine = node_manager_->getEngine(model);
        if (!engine) {
            return notFound("Model '" + model + "' not found", "model");
        }
        
        // Generate completion ID
        std::string completionId = generateCompletionId();
        
        ServerLogger::logInfo("Processing OpenAI chat completion for model '%s'", model.c_str());
        
        // Submit job
        int jobId = engine->submitChatCompletionsJob(params);
        if (jobId < 0) {
            return serverError("Failed to submit chat completion job");
        }
        
        // Handle streaming
        if (params.streaming && streaming_callback) {
            handleOpenAIStreaming(engine.get(), jobId, model, completionId, true, streaming_callback);
            return Response(200);  // Streaming handled via callback
        }
        
        // Wait for completion
        engine->waitForJob(jobId);
        
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            return serverError("Inference error: " + error);
        }
        
        // Get result and build response
        CompletionResult result = engine->getJobResult(jobId);
        nlohmann::json response = buildOpenAIChatResponse(result, model, completionId);
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing chat completion: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ChatCompletionController::processCompletion(
    const std::string& body,
    std::function<void(const std::string&)> streaming_callback) {
    
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return processCompletion(json, streaming_callback);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing completion: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ChatCompletionController::processCompletion(
    const nlohmann::json& request,
    std::function<void(const std::string&)> streaming_callback) {
    
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Extract model
        if (!request.contains("model") || !request["model"].is_string()) {
            return badRequest("Missing or invalid 'model' field", "model");
        }
        std::string model = request["model"];
        
        // Parse parameters
        CompletionParameters params = parseOpenAICompletionParameters(request);
        
        // Apply response_format if present
        applyResponseFormat(params, request);
        
        if (!params.isValid()) {
            return badRequest("Invalid completion parameters");
        }
        
        // Get engine
        auto engine = node_manager_->getEngine(model);
        if (!engine) {
            return notFound("Model '" + model + "' not found", "model");
        }
        
        // Generate completion ID
        std::string completionId = generateCompletionId();
        
        ServerLogger::logInfo("Processing OpenAI completion for model '%s'", model.c_str());
        
        // Submit job
        int jobId = engine->submitCompletionsJob(params);
        if (jobId < 0) {
            return serverError("Failed to submit completion job");
        }
        
        // Handle streaming
        if (params.streaming && streaming_callback) {
            handleOpenAIStreaming(engine.get(), jobId, model, completionId, false, streaming_callback);
            return Response(200);  // Streaming handled via callback
        }
        
        // Wait for completion
        engine->waitForJob(jobId);
        
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            return serverError("Inference error: " + error);
        }
        
        // Get result and build response
        CompletionResult result = engine->getJobResult(jobId);
        nlohmann::json response = buildOpenAICompletionResponse(result, model, completionId);
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing completion: %s", e.what());
        return serverError(e.what());
    }
}

ChatCompletionParameters ChatCompletionController::parseOpenAIChatParameters(const nlohmann::json& request) const {
    ChatCompletionParameters params;
    
    // Parse messages
    if (!request.contains("messages") || !request["messages"].is_array()) {
        throw std::invalid_argument("Missing or invalid 'messages' field");
    }
    
    for (const auto& msg : request["messages"]) {
        if (!msg.contains("role") || !msg.contains("content")) {
            throw std::invalid_argument("Invalid message format");
        }
        params.messages.emplace_back(msg["role"], msg["content"]);
    }
    
    // Parse parameters
    if (request.contains("temperature")) {
        params.temperature = request["temperature"];
    }
    if (request.contains("top_p")) {
        params.topP = request["top_p"];
    }
    if (request.contains("max_tokens")) {
        params.maxNewTokens = request["max_tokens"];
    }
    if (request.contains("stream")) {
        params.streaming = request["stream"];
    }
    if (request.contains("seed")) {
        params.randomSeed = request["seed"];
    }
    
    return params;
}

CompletionParameters ChatCompletionController::parseOpenAICompletionParameters(const nlohmann::json& request) const {
    CompletionParameters params;
    
    // Parse prompt
    if (!request.contains("prompt")) {
        throw std::invalid_argument("Missing 'prompt' field");
    }
    
    const auto& prompt = request["prompt"];
    if (prompt.is_string()) {
        params.prompt = prompt;
    } else if (prompt.is_array() && !prompt.empty()) {
        // Join array prompts
        std::ostringstream joined;
        for (size_t i = 0; i < prompt.size(); ++i) {
            if (prompt[i].is_string()) {
                joined << prompt[i].get<std::string>();
                if (i < prompt.size() - 1) joined << "\n";
            }
        }
        params.prompt = joined.str();
    } else {
        throw std::invalid_argument("Invalid 'prompt' format");
    }
    
    // Parse parameters
    if (request.contains("temperature")) {
        params.temperature = request["temperature"];
    }
    if (request.contains("top_p")) {
        params.topP = request["top_p"];
    }
    if (request.contains("max_tokens")) {
        params.maxNewTokens = request["max_tokens"];
    }
    if (request.contains("stream")) {
        params.streaming = request["stream"];
    }
    if (request.contains("seed")) {
        params.randomSeed = request["seed"];
    }
    
    return params;
}

nlohmann::json ChatCompletionController::buildOpenAIChatResponse(
    const CompletionResult& result,
    const std::string& model,
    const std::string& id,
    bool isStream) const {
    
    nlohmann::json response = {
        {"id", id},
        {"object", isStream ? "chat.completion.chunk" : "chat.completion"},
        {"created", std::chrono::system_clock::now().time_since_epoch().count() / 1000},
        {"model", model},
        {"choices", nlohmann::json::array()}
    };
    
    nlohmann::json choice = {
        {"index", 0},
        {"finish_reason", isStream ? nullptr : "stop"}
    };
    
    if (isStream) {
        choice["delta"] = {{"content", result.text}};
    } else {
        choice["message"] = {
            {"role", "assistant"},
            {"content", result.text}
        };
    }
    
    response["choices"].push_back(choice);
    
    if (!isStream) {
        response["usage"] = {
            {"prompt_tokens", result.prompt_token_count},
            {"completion_tokens", static_cast<int>(result.tokens.size())},
            {"total_tokens", result.prompt_token_count + static_cast<int>(result.tokens.size())}
        };
    }
    
    return response;
}

nlohmann::json ChatCompletionController::buildOpenAICompletionResponse(
    const CompletionResult& result,
    const std::string& model,
    const std::string& id,
    bool isStream) const {
    
    nlohmann::json response = {
        {"id", id},
        {"object", isStream ? "text_completion.chunk" : "text_completion"},
        {"created", std::chrono::system_clock::now().time_since_epoch().count() / 1000},
        {"model", model},
        {"choices", nlohmann::json::array()}
    };
    
    nlohmann::json choice = {
        {"text", result.text},
        {"index", 0},
        {"finish_reason", isStream ? nullptr : "stop"}
    };
    
    response["choices"].push_back(choice);
    
    if (!isStream) {
        response["usage"] = {
            {"prompt_tokens", result.prompt_token_count},
            {"completion_tokens", static_cast<int>(result.tokens.size())},
            {"total_tokens", result.prompt_token_count + static_cast<int>(result.tokens.size())}
        };
    }
    
    return response;
}

std::string ChatCompletionController::generateCompletionId() const {
    static std::atomic<uint64_t> counter{0};
    std::stringstream ss;
    ss << "chatcmpl-" << std::chrono::system_clock::now().time_since_epoch().count() 
       << "-" << counter++;
    return ss.str();
}

void ChatCompletionController::handleOpenAIStreaming(
    IInferenceEngine* engine,
    int jobId,
    const std::string& model,
    const std::string& id,
    bool isChat,
    std::function<void(const std::string&)> callback) const {
    
    std::string previousText = "";
    
    while (!engine->isJobFinished(jobId)) {
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            nlohmann::json errorChunk = {
                {"error", {{"message", error}, {"type", "server_error"}}}
            };
            callback("data: " + errorChunk.dump() + "\n\n");
            break;
        }
        
        CompletionResult result = engine->getJobResult(jobId);
        
        if (result.text.length() > previousText.length()) {
            std::string newContent = result.text.substr(previousText.length());
            
            // Build streaming chunk
            CompletionResult chunkResult;
            chunkResult.text = newContent;
            
            nlohmann::json chunk = isChat ? 
                buildOpenAIChatResponse(chunkResult, model, id, true) :
                buildOpenAICompletionResponse(chunkResult, model, id, true);
            
            callback("data: " + chunk.dump() + "\n\n");
            previousText = result.text;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Send completion marker
    callback("data: [DONE]\n\n");
}

void ChatCompletionController::applyResponseFormat(ChatCompletionParameters& params, const nlohmann::json& request) const {
    if (request.contains("response_format") && request["response_format"].is_object()) {
        const auto& rf = request["response_format"];
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            if (type == "json_object") {
                params.jsonSchema = "{\"type\":\"object\"}";
            } else if (type == "json_schema" && rf.contains("json_schema")) {
                const auto& schema = rf["json_schema"];
                if (schema.is_object()) {
                    params.jsonSchema = schema.dump();
                }
            }
        }
    }
}

void ChatCompletionController::applyResponseFormat(CompletionParameters& params, const nlohmann::json& request) const {
    if (request.contains("response_format") && request["response_format"].is_object()) {
        const auto& rf = request["response_format"];
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            if (type == "json_object") {
                params.jsonSchema = "{\"type\":\"object\"}";
            } else if (type == "json_schema" && rf.contains("json_schema")) {
                const auto& schema = rf["json_schema"];
                if (schema.is_object()) {
                    params.jsonSchema = schema.dump();
                }
            }
        }
    }
}

} // namespace controllers
} // namespace kolosal