#include "kolosal/controllers/completion_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <thread>
#include <chrono>

namespace kolosal {
namespace controllers {

CompletionController::CompletionController(NodeManager* node_manager)
    : node_manager_(node_manager) {
    if (!node_manager_) {
        try {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        } catch (...) {
            // NodeManager might not be available in some contexts
        }
    }
}

BaseController::Response CompletionController::processCompletion(const std::string& body,
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

BaseController::Response CompletionController::processCompletion(const nlohmann::json& request,
                                                                std::function<void(const std::string&)> streaming_callback) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Check if this is actually a chat completion request
        if (isChatCompletionRequest(request)) {
            return processChatCompletion(request, streaming_callback);
        }
        
        // Extract model name
        if (!request.contains("model") || !request["model"].is_string()) {
            return badRequest("Missing or invalid 'model' field", "model");
        }
        std::string modelName = request["model"];
        
        // Parse completion parameters
        CompletionParameters params = parseCompletionParameters(request);
        
        if (!params.isValid()) {
            return badRequest("Invalid completion parameters");
        }
        
        ServerLogger::logInfo("Processing text completion for model '%s'", modelName.c_str());
        
        // Get the inference engine
        auto engine = node_manager_->getEngine(modelName);
        if (!engine) {
            return notFound("Model '" + modelName + "' not found or could not be loaded", "model");
        }
        
        // Submit job
        int jobId = engine->submitCompletionsJob(params);
        if (jobId < 0) {
            return serverError("Failed to submit completion job to inference engine");
        }
        
        // Handle streaming if requested
        if (params.streaming && streaming_callback) {
            handleStreamingCompletion(engine.get(), jobId, streaming_callback);
            return Response(200);  // Streaming handled via callback
        }
        
        // Wait for non-streaming completion
        engine->waitForJob(jobId);
        
        // Check for errors
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            return serverError("Inference error: " + error);
        }
        
        // Get result
        CompletionResult result = engine->getJobResult(jobId);
        nlohmann::json response = completionResultToJson(result);
        
        ServerLogger::logInfo("Completed text completion (%.2f tokens/sec)", result.tps);
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing completion: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response CompletionController::processChatCompletion(const std::string& body,
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

BaseController::Response CompletionController::processChatCompletion(const nlohmann::json& request,
                                                                    std::function<void(const std::string&)> streaming_callback) {
    try {
        if (!node_manager_) {
            node_manager_ = &ServerAPI::instance().getNodeManager();
        }
        
        // Extract model name
        if (!request.contains("model") || !request["model"].is_string()) {
            return badRequest("Missing or invalid 'model' field", "model");
        }
        std::string modelName = request["model"];
        
        // Parse chat completion parameters
        ChatCompletionParameters params = parseChatCompletionParameters(request);
        
        if (!params.isValid()) {
            return badRequest("Invalid chat completion parameters");
        }
        
        ServerLogger::logInfo("Processing chat completion for model '%s' with seqId: %d", 
                            modelName.c_str(), params.seqId);
        
        // Get the inference engine
        auto engine = node_manager_->getEngine(modelName);
        if (!engine) {
            return notFound("Model '" + modelName + "' not found or could not be loaded", "model");
        }
        
        // Submit job
        int jobId = engine->submitChatCompletionsJob(params);
        if (jobId < 0) {
            return serverError("Failed to submit chat completion job to inference engine");
        }
        
        // Handle streaming if requested
        if (params.streaming && streaming_callback) {
            handleStreamingCompletion(engine.get(), jobId, streaming_callback);
            return Response(200);  // Streaming handled via callback
        }
        
        // Wait for non-streaming completion
        engine->waitForJob(jobId);
        
        // Check for errors
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            return serverError("Inference error: " + error);
        }
        
        // Get result
        CompletionResult result = engine->getJobResult(jobId);
        nlohmann::json response = completionResultToJson(result);
        
        ServerLogger::logInfo("Completed chat completion (%.2f tokens/sec)", result.tps);
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing chat completion: %s", e.what());
        return serverError(e.what());
    }
}

bool CompletionController::isChatCompletionRequest(const nlohmann::json& request) const {
    return request.contains("messages");
}

CompletionParameters CompletionController::parseCompletionParameters(const nlohmann::json& j) const {
    CompletionParameters params;
    
    // Required field
    if (!j.contains("prompt") || !j["prompt"].is_string()) {
        throw std::invalid_argument("Missing or invalid 'prompt' field");
    }
    params.prompt = j["prompt"];
    
    // Optional fields
    if (j.contains("randomSeed") && j["randomSeed"].is_number_integer()) {
        params.randomSeed = j["randomSeed"];
    }
    if (j.contains("maxNewTokens") && j["maxNewTokens"].is_number_integer()) {
        params.maxNewTokens = j["maxNewTokens"];
    }
    if (j.contains("temperature") && j["temperature"].is_number()) {
        params.temperature = j["temperature"];
    }
    if (j.contains("topP") && j["topP"].is_number()) {
        params.topP = j["topP"];
    }
    if (j.contains("streaming") && j["streaming"].is_boolean()) {
        params.streaming = j["streaming"];
    }
    if (j.contains("grammar") && j["grammar"].is_string()) {
        params.grammar = j["grammar"];
    }
    
    // Handle JSON schema
    if (j.contains("jsonSchema")) {
        if (j["jsonSchema"].is_string()) {
            params.jsonSchema = j["jsonSchema"];
        } else if (j["jsonSchema"].is_object()) {
            params.jsonSchema = j["jsonSchema"].dump();
        }
    }
    
    // Handle OpenAI response_format
    if (j.contains("response_format") && j["response_format"].is_object()) {
        const auto& rf = j["response_format"];
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            if (type == "json_object") {
                params.jsonSchema = "{\"type\":\"object\"}";
            } else if (type == "json_schema" && rf.contains("json_schema")) {
                const auto& js = rf["json_schema"];
                if (js.contains("schema") && js["schema"].is_object()) {
                    params.jsonSchema = js["schema"].dump();
                } else if (js.is_object()) {
                    params.jsonSchema = js.dump();
                }
            }
        }
    }
    
    finalizeStructuredOutput(params, "completion");
    return params;
}

ChatCompletionParameters CompletionController::parseChatCompletionParameters(const nlohmann::json& j) const {
    ChatCompletionParameters params;
    
    // Required field: messages
    if (!j.contains("messages") || !j["messages"].is_array()) {
        throw std::invalid_argument("Missing or invalid 'messages' field");
    }
    
    for (const auto& msgJson : j["messages"]) {
        if (!msgJson.contains("role") || !msgJson.contains("content") ||
            !msgJson["role"].is_string() || !msgJson["content"].is_string()) {
            throw std::invalid_argument("Invalid message format in messages array");
        }
        
        Message msg(msgJson["role"], msgJson["content"]);
        params.messages.push_back(msg);
    }
    
    // Optional fields
    if (j.contains("randomSeed") && j["randomSeed"].is_number_integer()) {
        params.randomSeed = j["randomSeed"];
    }
    if (j.contains("maxNewTokens") && j["maxNewTokens"].is_number_integer()) {
        params.maxNewTokens = j["maxNewTokens"];
    }
    if (j.contains("temperature") && j["temperature"].is_number()) {
        params.temperature = j["temperature"];
    }
    if (j.contains("topP") && j["topP"].is_number()) {
        params.topP = j["topP"];
    }
    if (j.contains("streaming") && j["streaming"].is_boolean()) {
        params.streaming = j["streaming"];
    }
    if (j.contains("seqId") && j["seqId"].is_number_integer()) {
        params.seqId = j["seqId"];
    }
    if (j.contains("tools") && j["tools"].is_string()) {
        params.tools = j["tools"];
    }
    if (j.contains("toolChoice") && j["toolChoice"].is_string()) {
        params.toolChoice = j["toolChoice"];
    }
    if (j.contains("grammar") && j["grammar"].is_string()) {
        params.grammar = j["grammar"];
    }
    
    // Handle JSON schema
    if (j.contains("jsonSchema")) {
        if (j["jsonSchema"].is_string()) {
            params.jsonSchema = j["jsonSchema"];
        } else if (j["jsonSchema"].is_object()) {
            params.jsonSchema = j["jsonSchema"].dump();
        }
    }
    
    // Handle OpenAI response_format
    if (j.contains("response_format") && j["response_format"].is_object()) {
        const auto& rf = j["response_format"];
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            if (type == "json_object") {
                params.jsonSchema = "{\"type\":\"object\"}";
            } else if (type == "json_schema" && rf.contains("json_schema")) {
                const auto& js = rf["json_schema"];
                if (js.contains("schema") && js["schema"].is_object()) {
                    params.jsonSchema = js["schema"].dump();
                } else if (js.is_object()) {
                    params.jsonSchema = js.dump();
                }
            }
        }
    }
    
    finalizeStructuredOutput(params, "chat");
    return params;
}

nlohmann::json CompletionController::completionResultToJson(const CompletionResult& result) const {
    return {
        {"tokens", result.tokens},
        {"text", result.text},
        {"tps", result.tps},
        {"ttft", result.ttft},
        {"prompt_tokens", result.prompt_token_count},
        {"completion_tokens", static_cast<int>(result.tokens.size())},
        {"total_tokens", result.prompt_token_count + static_cast<int>(result.tokens.size())}
    };
}

template<typename P>
void CompletionController::finalizeStructuredOutput(P& params, const char* context) const {
    // Precedence: explicit grammar overrides jsonSchema-derived grammar
    if (!params.grammar.empty()) {
        if (!params.jsonSchema.empty()) {
            ServerLogger::logInfo("[%s] Both grammar & jsonSchema provided; grammar takes precedence", context);
        } else {
            ServerLogger::logInfo("[%s] Using provided grammar (chars=%zu)", context, params.grammar.size());
        }
    } else if (!params.jsonSchema.empty()) {
        ServerLogger::logInfo("[%s] Using provided JSON schema (chars=%zu)", context, params.jsonSchema.size());
    }
}

void CompletionController::handleStreamingCompletion(IInferenceEngine* engine, int jobId,
                                                    std::function<void(const std::string&)> callback) const {
    std::string previousText = "";
    size_t lastTokenCount = 0;
    
    while (!engine->isJobFinished(jobId)) {
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            nlohmann::json errorResponse = {
                {"error", error},
                {"text", ""},
                {"tokens", nlohmann::json::array()}
            };
            
            // Send error as SSE data
            callback("data: " + errorResponse.dump() + "\n\n");
            break;
        }
        
        CompletionResult result = engine->getJobResult(jobId);
        
        // Check for new content
        if (result.text.length() > previousText.length()) {
            std::string newContent = result.text.substr(previousText.length());
            
            // Create partial result
            CompletionResult partialResult;
            partialResult.text = newContent;
            partialResult.tps = result.tps;
            partialResult.ttft = result.ttft;
            
            if (result.tokens.size() > lastTokenCount) {
                auto startIt = result.tokens.begin() + static_cast<long>(lastTokenCount);
                partialResult.tokens.assign(startIt, result.tokens.end());
            }
            partialResult.prompt_token_count = result.prompt_token_count;
            
            nlohmann::json streamResponse = completionResultToJson(partialResult);
            streamResponse["partial"] = true;
            
            // Send as SSE data
            callback("data: " + streamResponse.dump() + "\n\n");
            
            previousText = result.text;
            lastTokenCount = result.tokens.size();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Send completion marker
    callback("data: [DONE]\n\n");
}

// Explicit instantiation for the template
template void CompletionController::finalizeStructuredOutput<CompletionParameters>(
    CompletionParameters& params, const char* context) const;
template void CompletionController::finalizeStructuredOutput<ChatCompletionParameters>(
    ChatCompletionParameters& params, const char* context) const;

} // namespace controllers
} // namespace kolosal