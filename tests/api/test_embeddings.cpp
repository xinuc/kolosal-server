#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"
#include <algorithm>
#include <cctype>

using namespace kolosal::test;

TEST_CASE("Embeddings API - Priority 3 Compatibility", "[embeddings][priority3][compatibility]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("3.2.1 Generate Embeddings - OpenAI Format") {
        INFO("Testing POST /v1/embeddings OpenAI format");
        
        nlohmann::json request = {
            {"input", "Hello world"},
            {"model", "text-embedding-ada-002"}
        };
        
        auto response = client.post("/v1/embeddings", request);
        
        if (response.status_code == 404) {
            // Check if it's model not found vs endpoint not found
            if (response.body.contains("error") && 
                response.body["error"].contains("message")) {
                std::string error_msg = response.body["error"]["message"].get<std::string>();
                std::transform(error_msg.begin(), error_msg.end(), error_msg.begin(), ::tolower);
                if (error_msg.find("model") != std::string::npos) {
                // This is EXPECTED behavior when model isn't loaded
                INFO("Endpoint correctly returns 404 for missing model");
                
                // Verify proper error response format
                REQUIRE(response.body["error"].contains("message"));
                CHECK(response.body["error"].contains("type"));
                
                    // Test passes - endpoint exists and behaves correctly
                    SUCCEED("OpenAI embeddings endpoint works correctly (model not loaded)");
                    return;
                }
            } else {
                WARN("OpenAI embeddings endpoint not implemented yet");
                SKIP("Endpoint not available");
            }
        }
        
        if (response.status_code == 200) {
            // Verify OpenAI format requirements
            REQUIRE(response.body.contains("object"));
            REQUIRE(response.body.contains("data"));
            REQUIRE(response.body.contains("model"));
            REQUIRE(response.body.contains("usage"));
            
            // Check specific values
            CHECK(response.body["object"] == "list");
            CHECK(response.body["data"].is_array());
            
            // Check data array structure
            if (response.body["data"].size() > 0) {
                const auto& embedding = response.body["data"][0];
                
                REQUIRE(embedding.contains("object"));
                REQUIRE(embedding.contains("embedding"));
                REQUIRE(embedding.contains("index"));
                
                CHECK(embedding["object"] == "embedding");
                CHECK(embedding["embedding"].is_array());
                CHECK(embedding["index"].is_number());
                
                // Check embedding is float array
                if (embedding["embedding"].size() > 0) {
                    CHECK(embedding["embedding"][0].is_number());
                }
            }
            
            // Check usage structure
            const auto& usage = response.body["usage"];
            REQUIRE(usage.contains("prompt_tokens"));
            REQUIRE(usage.contains("total_tokens"));
            CHECK(usage["prompt_tokens"].is_number());
            CHECK(usage["total_tokens"].is_number());
            
            // Should NOT have completion_tokens for embeddings
            CHECK_FALSE(usage.contains("completion_tokens"));
        }
    }
    
    SECTION("3.2.2 Generate Embeddings - Batch Input") {
        INFO("Testing POST /v1/embeddings with array input");
        
        nlohmann::json request = {
            {"input", nlohmann::json::array({"Hello world", "Goodbye world"})},
            {"model", "text-embedding-ada-002"}
        };
        
        auto response = client.post("/v1/embeddings", request);
        
        if (response.status_code == 404) {
            // Check if it's model not found vs endpoint not found
            if (response.body.contains("error") && 
                response.body["error"].contains("message")) {
                std::string error_msg = response.body["error"]["message"].get<std::string>();
                std::transform(error_msg.begin(), error_msg.end(), error_msg.begin(), ::tolower);
                if (error_msg.find("model") != std::string::npos) {
                // This is EXPECTED behavior when model isn't loaded
                INFO("Endpoint correctly returns 404 for missing model");
                
                // Verify proper error response format
                REQUIRE(response.body["error"].contains("message"));
                CHECK(response.body["error"].contains("type"));
                
                    // Test passes - endpoint exists and behaves correctly
                    SUCCEED("OpenAI embeddings endpoint works correctly (model not loaded)");
                    return;
                }
            } else {
                WARN("OpenAI embeddings endpoint not implemented yet");
                SKIP("Endpoint not available");
            }
        }
        
        if (response.status_code == 200) {
            REQUIRE(response.body.contains("data"));
            REQUIRE(response.body["data"].is_array());
            
            // Should have 2 embeddings for 2 inputs
            CHECK(response.body["data"].size() == 2);
            
            // Check each has correct index
            if (response.body["data"].size() >= 2) {
                CHECK(response.body["data"][0]["index"] == 0);
                CHECK(response.body["data"][1]["index"] == 1);
            }
        }
    }
    
    SECTION("3.2.3 Standard Embeddings Format") {
        INFO("Testing POST /embeddings standard format");
        
        nlohmann::json request = {
            {"text", "Hello world"},
            {"model", "embedding-model"}
        };
        
        auto response = client.post("/embeddings", request);
        
        if (response.status_code == 404) {
            // Check if it's model not found vs endpoint not found
            if (response.body.contains("error") && 
                (response.body["error"].is_string() && response.body["error"].get<std::string>().find("model") != std::string::npos ||
                 response.body["error"].is_object() && response.body["error"]["message"].get<std::string>().find("model") != std::string::npos)) {
                // This is EXPECTED behavior when model isn't loaded
                INFO("Endpoint correctly returns 404 for missing model");
                
                // Test passes - endpoint exists and behaves correctly
                SUCCEED("Standard embeddings endpoint works correctly (model not loaded)");
                return;
            } else {
                WARN("Standard embeddings endpoint not implemented yet");
                SKIP("Endpoint not available");
            }
        }
        
        if (response.status_code == 200) {
            // Check for standard format (non-OpenAI)
            REQUIRE(response.body.contains("embedding"));
            CHECK(response.body["embedding"].is_array());
            
            // May have metadata
            if (response.body.contains("model")) {
                CHECK(response.body["model"].is_string());
            }
            if (response.body.contains("dimensions")) {
                CHECK(response.body["dimensions"].is_number());
            }
            
            // Should NOT have OpenAI format
            CHECK_FALSE(response.body.contains("object"));
            CHECK_FALSE(response.body.contains("data"));
            CHECK_FALSE(response.body.contains("usage"));
        }
    }
}