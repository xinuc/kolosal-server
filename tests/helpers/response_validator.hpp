#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace kolosal {
namespace test {

class ResponseValidator {
public:
    // Field validation
    static void checkFieldExists(const nlohmann::json& j, const std::string& field);
    static void checkFieldType(const nlohmann::json& j, const std::string& field, nlohmann::json::value_t expected_type);
    static void checkFieldValue(const nlohmann::json& j, const std::string& field, const nlohmann::json& expected_value);
    
    // Structure validation
    static void checkRequiredFields(const nlohmann::json& j, const std::vector<std::string>& fields);
    static void checkExactFields(const nlohmann::json& j, const std::vector<std::string>& fields);
    
    // Error response validation
    static void checkErrorResponse(const nlohmann::json& j);
    static void checkErrorStructure(const nlohmann::json& j, const std::string& expected_type = "", const std::string& expected_param = "");
    
    // Success response validation
    static void checkSuccessResponse(const nlohmann::json& j);
    static void checkMessageResponse(const nlohmann::json& j, const std::string& expected_message = "");
    
    // OpenAI format validation
    static void checkOpenAIModelsList(const nlohmann::json& j);
    static void checkOpenAICompletion(const nlohmann::json& j);
    static void checkOpenAIChatCompletion(const nlohmann::json& j);
    static void checkOpenAIEmbedding(const nlohmann::json& j);
    
    // Standard format validation
    static void checkStandardModelsList(const nlohmann::json& j);
    static void checkStandardCompletion(const nlohmann::json& j);
    static void checkStandardChatCompletion(const nlohmann::json& j);
    static void checkStandardEmbedding(const nlohmann::json& j);
    
    // Downloads validation
    static void checkDownloadsList(const nlohmann::json& j);
    static void checkDownloadProgress(const nlohmann::json& j);
    
    // Documents validation
    static void checkDocumentsList(const nlohmann::json& j);
    static void checkDocumentsInfo(const nlohmann::json& j);
    static void checkRetrieveResponse(const nlohmann::json& j);
    
    // Auth validation
    static void checkAuthConfig(const nlohmann::json& j);
    static void checkAuthStats(const nlohmann::json& j);
    
    // Server config validation
    static void checkServerConfig(const nlohmann::json& j);
    static void checkServerStats(const nlohmann::json& j);
    
    // Engines validation
    static void checkEnginesList(const nlohmann::json& j);
    
    // Chunking validation
    static void checkChunkingResponse(const nlohmann::json& j);

    // Utility to compare two JSON objects for compatibility
    static bool areCompatible(const nlohmann::json& old_response, const nlohmann::json& new_response, std::string& diff);
};

// Catch2 matchers for better error messages
class ContainsField : public Catch::Matchers::MatcherBase<nlohmann::json> {
    std::string field_;
public:
    ContainsField(const std::string& field) : field_(field) {}
    bool match(const nlohmann::json& j) const override {
        return j.contains(field_);
    }
    std::string describe() const override {
        return "contains field '" + field_ + "'";
    }
};

class HasFieldType : public Catch::Matchers::MatcherBase<nlohmann::json> {
    std::string field_;
    nlohmann::json::value_t type_;
public:
    HasFieldType(const std::string& field, nlohmann::json::value_t type) 
        : field_(field), type_(type) {}
    bool match(const nlohmann::json& j) const override {
        return j.contains(field_) && j[field_].type() == type_;
    }
    std::string describe() const override {
        return "has field '" + field_ + "' with correct type";
    }
};

// Helper functions for creating matchers
inline ContainsField HasField(const std::string& field) {
    return ContainsField(field);
}

inline HasFieldType HasFieldWithType(const std::string& field, nlohmann::json::value_t type) {
    return HasFieldType(field, type);
}

} // namespace test
} // namespace kolosal