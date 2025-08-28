#include "kolosal/controllers/controller_utils.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>

namespace kolosal {
namespace controllers {
namespace utils {

const std::vector<std::string> ControllerUtils::SUPPORTED_MODEL_TYPES = {
    "llm",
    "embedding"
};

void ControllerUtils::applyResponseFormat(ChatCompletionParameters& params, const nlohmann::json& request) {
    if (request.contains("response_format") && request["response_format"].is_object()) {
        const auto& rf = request["response_format"];
        
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            
            if (type == "json_object" || type == "json_schema") {
                params.jsonSchema = "";
                
                if (type == "json_schema" && rf.contains("json_schema") && rf["json_schema"].is_object()) {
                    const auto& schemaObj = rf["json_schema"];
                    
                    if (schemaObj.contains("schema") && (schemaObj["schema"].is_object() || schemaObj["schema"].is_string())) {
                        if (schemaObj["schema"].is_string()) {
                            params.jsonSchema = schemaObj["schema"];
                        } else {
                            params.jsonSchema = schemaObj["schema"].dump();
                        }
                        ServerLogger::logInfo("Using JSON schema for structured output (chars=%zu)", 
                                            params.jsonSchema.size());
                    }
                } else if (type == "json_object") {
                    // Simple JSON object mode - no specific schema
                    params.jsonSchema = R"({"type":"object"})";
                    ServerLogger::logInfo("Using simple JSON object mode for structured output");
                }
            }
        }
    }
}

void ControllerUtils::applyResponseFormat(CompletionParameters& params, const nlohmann::json& request) {
    if (request.contains("response_format") && request["response_format"].is_object()) {
        const auto& rf = request["response_format"];
        
        if (rf.contains("type") && rf["type"].is_string()) {
            const std::string type = rf["type"];
            
            if (type == "json_object" || type == "json_schema") {
                params.jsonSchema = "";
                
                if (type == "json_schema" && rf.contains("json_schema") && rf["json_schema"].is_object()) {
                    const auto& schemaObj = rf["json_schema"];
                    
                    if (schemaObj.contains("schema") && (schemaObj["schema"].is_object() || schemaObj["schema"].is_string())) {
                        if (schemaObj["schema"].is_string()) {
                            params.jsonSchema = schemaObj["schema"];
                        } else {
                            params.jsonSchema = schemaObj["schema"].dump();
                        }
                        ServerLogger::logInfo("Using JSON schema for structured output (chars=%zu)", 
                                            params.jsonSchema.size());
                    }
                } else if (type == "json_object") {
                    // Simple JSON object mode - no specific schema
                    params.jsonSchema = R"({"type":"object"})";
                    ServerLogger::logInfo("Using simple JSON object mode for structured output");
                }
            }
        }
    }
}

size_t ControllerUtils::estimateTokenCount(const std::string& text) {
    // Simple estimation: average 4 characters per token
    // This is a rough approximation that works reasonably well for English text
    return (text.length() + CHARS_PER_TOKEN - 1) / CHARS_PER_TOKEN; // Round up
}

void ControllerUtils::parseCommonParameters(CompletionParameters& params, const nlohmann::json& j) {
    // Parse common parameters shared by all completion types
    if (j.contains("max_tokens") && j["max_tokens"].is_number_integer()) {
        params.maxNewTokens = j["max_tokens"];
    }
    if (j.contains("temperature") && j["temperature"].is_number()) {
        params.temperature = j["temperature"];
    }
    if (j.contains("top_p") && j["top_p"].is_number()) {
        params.topP = j["top_p"];
    }
    if (j.contains("top_k") && j["top_k"].is_number_integer()) {
        // top_k parameter requested but not supported in current CompletionParameters
        // Silently ignore to maintain backward compatibility
    }
    if (j.contains("stop") && j["stop"].is_array()) {
        // stop words requested but not supported in current CompletionParameters
        // Silently ignore to maintain backward compatibility
    }
    if (j.contains("seed") && j["seed"].is_number_integer()) {
        params.randomSeed = j["seed"];
    }
    if (j.contains("repeat_penalty") && j["repeat_penalty"].is_number()) {
        // repeat_penalty requested but not supported in current CompletionParameters
        // Silently ignore to maintain backward compatibility
    }
}

void ControllerUtils::parseCommonParameters(ChatCompletionParameters& params, const nlohmann::json& j) {
    // Parse common parameters shared by all completion types
    if (j.contains("max_tokens") && j["max_tokens"].is_number_integer()) {
        params.maxNewTokens = j["max_tokens"];
    }
    if (j.contains("temperature") && j["temperature"].is_number()) {
        params.temperature = j["temperature"];
    }
    if (j.contains("top_p") && j["top_p"].is_number()) {
        params.topP = j["top_p"];
    }
    if (j.contains("top_k") && j["top_k"].is_number_integer()) {
        // top_k parameter requested but not supported in current ChatCompletionParameters
        // Silently ignore to maintain backward compatibility
    }
    if (j.contains("stop") && j["stop"].is_array()) {
        // stop words requested but not supported in current ChatCompletionParameters
        // Silently ignore to maintain backward compatibility
    }
    if (j.contains("seed") && j["seed"].is_number_integer()) {
        params.randomSeed = j["seed"];
    }
    if (j.contains("repeat_penalty") && j["repeat_penalty"].is_number()) {
        // repeat_penalty requested but not supported in current ChatCompletionParameters
        // Silently ignore to maintain backward compatibility
    }
}

bool ControllerUtils::isValidModelType(const std::string& modelType) {
    return std::find(SUPPORTED_MODEL_TYPES.begin(), SUPPORTED_MODEL_TYPES.end(), modelType) 
           != SUPPORTED_MODEL_TYPES.end();
}

std::vector<std::string> ControllerUtils::getSupportedModelTypes() {
    return SUPPORTED_MODEL_TYPES;
}

} // namespace utils
} // namespace controllers
} // namespace kolosal