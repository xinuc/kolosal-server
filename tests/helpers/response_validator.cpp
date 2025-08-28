#include "response_validator.hpp"
#include <algorithm>
#include <sstream>

namespace kolosal {
namespace test {

void ResponseValidator::checkFieldExists(const nlohmann::json& j, const std::string& field) {
    REQUIRE(j.contains(field));
}

void ResponseValidator::checkFieldType(const nlohmann::json& j, const std::string& field, nlohmann::json::value_t expected_type) {
    REQUIRE(j.contains(field));
    REQUIRE(j[field].type() == expected_type);
}

void ResponseValidator::checkFieldValue(const nlohmann::json& j, const std::string& field, const nlohmann::json& expected_value) {
    REQUIRE(j.contains(field));
    REQUIRE(j[field] == expected_value);
}

void ResponseValidator::checkRequiredFields(const nlohmann::json& j, const std::vector<std::string>& fields) {
    for (const auto& field : fields) {
        REQUIRE(j.contains(field));
    }
}

void ResponseValidator::checkExactFields(const nlohmann::json& j, const std::vector<std::string>& fields) {
    // Check all required fields exist
    checkRequiredFields(j, fields);
    
    // Check no extra fields exist
    for (auto& [key, value] : j.items()) {
        bool found = std::find(fields.begin(), fields.end(), key) != fields.end();
        REQUIRE(found);
    }
}

void ResponseValidator::checkErrorResponse(const nlohmann::json& j) {
    checkFieldExists(j, "error");
    REQUIRE(j["error"].is_object());
    checkFieldExists(j["error"], "message");
    REQUIRE(j["error"]["message"].is_string());
}

void ResponseValidator::checkErrorStructure(const nlohmann::json& j, const std::string& expected_type, const std::string& expected_param) {
    checkErrorResponse(j);
    
    if (!expected_type.empty()) {
        checkFieldExists(j["error"], "type");
        checkFieldValue(j["error"], "type", expected_type);
    }
    
    if (!expected_param.empty()) {
        checkFieldExists(j["error"], "param");
        checkFieldValue(j["error"], "param", expected_param);
    }
}

void ResponseValidator::checkSuccessResponse(const nlohmann::json& j) {
    REQUIRE_FALSE(j.contains("error"));
}

void ResponseValidator::checkMessageResponse(const nlohmann::json& j, const std::string& expected_message) {
    checkFieldExists(j, "message");
    REQUIRE(j["message"].is_string());
    
    if (!expected_message.empty()) {
        REQUIRE(j["message"] == expected_message);
    }
}

// OpenAI format validation
void ResponseValidator::checkOpenAIModelsList(const nlohmann::json& j) {
    checkFieldExists(j, "object");
    checkFieldValue(j, "object", "list");
    checkFieldExists(j, "data");
    REQUIRE(j["data"].is_array());
    
    if (!j["data"].empty()) {
        for (const auto& model : j["data"]) {
            checkFieldExists(model, "id");
            checkFieldExists(model, "object");
            checkFieldValue(model, "object", "model");
            checkFieldExists(model, "created");
            checkFieldExists(model, "owned_by");
        }
    }
}

void ResponseValidator::checkOpenAICompletion(const nlohmann::json& j) {
    checkRequiredFields(j, {"id", "object", "created", "model", "choices", "usage"});
    checkFieldValue(j, "object", "text_completion");
    REQUIRE(j["choices"].is_array());
    REQUIRE(j["usage"].is_object());
}

void ResponseValidator::checkOpenAIChatCompletion(const nlohmann::json& j) {
    checkRequiredFields(j, {"id", "object", "created", "model", "choices", "usage"});
    checkFieldValue(j, "object", "chat.completion");
    REQUIRE(j["choices"].is_array());
    REQUIRE(j["usage"].is_object());
}

void ResponseValidator::checkOpenAIEmbedding(const nlohmann::json& j) {
    checkRequiredFields(j, {"object", "data", "model", "usage"});
    checkFieldValue(j, "object", "list");
    REQUIRE(j["data"].is_array());
    REQUIRE(j["usage"].is_object());
}

// Standard format validation
void ResponseValidator::checkStandardModelsList(const nlohmann::json& j) {
    checkFieldExists(j, "models");
    REQUIRE(j["models"].is_array());
    checkFieldExists(j, "total_count");
    checkFieldExists(j, "summary");
}

void ResponseValidator::checkStandardCompletion(const nlohmann::json& j) {
    checkFieldExists(j, "text");
    REQUIRE(j["text"].is_string());
}

void ResponseValidator::checkStandardChatCompletion(const nlohmann::json& j) {
    checkFieldExists(j, "text");
    REQUIRE(j["text"].is_string());
}

void ResponseValidator::checkStandardEmbedding(const nlohmann::json& j) {
    checkFieldExists(j, "embedding");
    REQUIRE(j["embedding"].is_array());
}

// Downloads validation
void ResponseValidator::checkDownloadsList(const nlohmann::json& j) {
    checkFieldExists(j, "active_downloads");
    REQUIRE(j["active_downloads"].is_array());
    checkFieldExists(j, "summary");
    REQUIRE(j["summary"].is_object());
    
    const auto& summary = j["summary"];
    checkRequiredFields(summary, {
        "total_active", 
        "startup_downloads", 
        "regular_downloads",
        "embedding_model_downloads",
        "llm_model_downloads"
    });
}

void ResponseValidator::checkDownloadProgress(const nlohmann::json& j) {
    checkRequiredFields(j, {"model_id", "type", "status", "url", "local_path", "progress", "timing"});
    
    const auto& progress = j["progress"];
    checkRequiredFields(progress, {"downloaded_bytes", "total_bytes", "percentage", "download_speed_bps"});
    
    const auto& timing = j["timing"];
    checkRequiredFields(timing, {"start_time", "elapsed_seconds"});
}

// Documents validation
void ResponseValidator::checkDocumentsList(const nlohmann::json& j) {
    checkRequiredFields(j, {"document_ids", "total_count", "collection_name"});
    REQUIRE(j["document_ids"].is_array());
}

void ResponseValidator::checkDocumentsInfo(const nlohmann::json& j) {
    checkRequiredFields(j, {"documents", "found_count", "not_found_count", "not_found_ids", "collection_name"});
    REQUIRE(j["documents"].is_array());
    REQUIRE(j["not_found_ids"].is_array());
}

void ResponseValidator::checkRetrieveResponse(const nlohmann::json& j) {
    checkRequiredFields(j, {"documents", "total_found", "query"});
    REQUIRE(j["documents"].is_array());
}

// Auth validation
void ResponseValidator::checkAuthConfig(const nlohmann::json& j) {
    checkRequiredFields(j, {"rate_limiter", "cors", "api_key"});
    
    const auto& rl = j["rate_limiter"];
    checkRequiredFields(rl, {"enabled", "max_requests", "window_size"});
    
    const auto& cors = j["cors"];
    checkRequiredFields(cors, {"enabled", "allowed_origins", "allowed_methods", "allowed_headers", "allow_credentials", "max_age"});
    
    const auto& api = j["api_key"];
    checkRequiredFields(api, {"enabled", "required", "header_name", "keys_count"});
}

void ResponseValidator::checkAuthStats(const nlohmann::json& j) {
    checkRequiredFields(j, {"rate_limit_stats", "cors_stats"});
    
    const auto& rl = j["rate_limit_stats"];
    checkRequiredFields(rl, {"total_clients", "total_requests", "clients"});
}

// Server config validation
void ResponseValidator::checkServerConfig(const nlohmann::json& j) {
    checkRequiredFields(j, {"server", "default_model_settings", "security", "performance"});
}

void ResponseValidator::checkServerStats(const nlohmann::json& j) {
    checkRequiredFields(j, {"system", "server", "models"});
}

// Engines validation
void ResponseValidator::checkEnginesList(const nlohmann::json& j) {
    checkRequiredFields(j, {"engines", "total_count", "loaded_count"});
    REQUIRE(j["engines"].is_array());
}

// Chunking validation
void ResponseValidator::checkChunkingResponse(const nlohmann::json& j) {
    checkFieldExists(j, "chunks");
    REQUIRE(j["chunks"].is_array());
}

// Utility to compare two JSON objects for compatibility
bool ResponseValidator::areCompatible(const nlohmann::json& old_response, const nlohmann::json& new_response, std::string& diff) {
    std::stringstream ss;
    bool compatible = true;
    
    // Check all fields in old response exist in new response
    for (auto& [key, old_value] : old_response.items()) {
        if (!new_response.contains(key)) {
            ss << "Missing field: " << key << "\n";
            compatible = false;
        } else if (old_value.type() != new_response[key].type()) {
            ss << "Type mismatch for field '" << key << "': " 
               << "old=" << old_value.type_name() 
               << " new=" << new_response[key].type_name() << "\n";
            compatible = false;
        } else if (old_value.is_object() && new_response[key].is_object()) {
            // Recursively check nested objects
            std::string nested_diff;
            if (!areCompatible(old_value, new_response[key], nested_diff)) {
                ss << "In field '" << key << "':\n" << nested_diff;
                compatible = false;
            }
        }
    }
    
    diff = ss.str();
    return compatible;
}

} // namespace test
} // namespace kolosal