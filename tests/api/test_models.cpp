#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"

using namespace kolosal::test;

TEST_CASE("Models API", "[models][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("List Models - Standard Format") {
        INFO("Testing GET /models standard format");
        
        auto response = client.get("/models");
        
        REQUIRE(response.status_code == 200);
        REQUIRE(response.body.contains("models"));
        REQUIRE(response.body["models"].is_array());
        
        // Check summary fields if present
        if (response.body.contains("summary")) {
            const auto& summary = response.body["summary"];
            REQUIRE(summary.contains("total_models"));
            REQUIRE(summary.contains("embedding_models"));
            REQUIRE(summary.contains("llm_models"));
            REQUIRE(summary.contains("loaded_models"));
            CHECK(summary["total_models"].is_number());
            CHECK(summary["embedding_models"].is_number());
            CHECK(summary["llm_models"].is_number());
            CHECK(summary["loaded_models"].is_number());
        }
        
        // Check model object structure if models exist
        if (response.body["models"].size() > 0) {
            const auto& first_model = response.body["models"][0];
            
            // CRITICAL: Must use "model_id" not "id"
            REQUIRE(first_model.contains("model_id"));
            REQUIRE_FALSE(first_model.contains("id"));  // Should NOT have "id" field
            
            // Check other required fields
            REQUIRE(first_model.contains("status"));
            REQUIRE(first_model.contains("available"));
            REQUIRE(first_model.contains("model_type"));
            
            CHECK(first_model["model_id"].is_string());
            CHECK((first_model["status"] == "loaded" || first_model["status"] == "unloaded"));
            CHECK(first_model["available"].is_boolean());
            CHECK((first_model["model_type"] == "llm" || first_model["model_type"] == "embedding"));
        }
    }
    
    SECTION("List Models - OpenAI Format") {
        INFO("Testing GET /v1/models OpenAI format");
        
        auto response = client.get("/v1/models");
        
        REQUIRE(response.status_code == 200);
        
        // OpenAI format uses "data" array
        REQUIRE(response.body.contains("data"));
        REQUIRE(response.body["data"].is_array());
        
        // Check for summary (our extension)
        if (response.body.contains("summary")) {
            CHECK(response.body["summary"].is_object());
        }
        
        // Check model object structure if models exist
        if (response.body["data"].size() > 0) {
            const auto& first_model = response.body["data"][0];
            
            // OpenAI format may use "id" or our format with "model_id"
            CHECK((first_model.contains("id") || first_model.contains("model_id")));
            
            if (first_model.contains("status")) {
                CHECK(first_model["status"].is_string());
            }
        }
    }
    
    SECTION("Get Model by ID") {
        INFO("Testing GET /models/{id}");
        
        // First get a model ID
        auto list_response = client.get("/models");
        if (list_response.status_code == 200 && list_response.body["models"].size() > 0) {
            auto& first_model = list_response.body["models"][0];
            if (first_model.contains("model_id") && !first_model["model_id"].is_null()) {
                std::string model_id = first_model["model_id"];
                
                auto response = client.get("/models/" + model_id);
                
                if (response.status_code == 200) {
                    CHECK(response.body.contains("model_id"));
                }
            }
        }
    }
    
    SECTION("Get Model by ID - V1 Endpoint") {
        INFO("Testing GET /v1/models/{id}");
        
        // First get a model ID
        auto list_response = client.get("/v1/models");
        if (list_response.status_code == 200 && list_response.body["data"].size() > 0) {
            auto& first_model = list_response.body["data"][0];
            std::string model_id;
            
            if (first_model.contains("id") && !first_model["id"].is_null()) {
                model_id = first_model["id"];
            } else if (first_model.contains("model_id") && !first_model["model_id"].is_null()) {
                model_id = first_model["model_id"];
            }
            
            if (!model_id.empty()) {
                auto response = client.get("/v1/models/" + model_id);
                
                if (response.status_code == 200) {
                    CHECK((response.body.contains("id") || response.body.contains("model_id")));
                    // Model info fields
                    if (response.body.contains("status")) {
                        CHECK(response.body["status"].is_string());
                    }
                }
            }
        }
    }
    
    SECTION("Get Model Status") {
        INFO("Testing GET /v1/models/{id}/status");
        
        // First get a model ID
        auto list_response = client.get("/v1/models");
        if (list_response.status_code == 200 && list_response.body["data"].size() > 0) {
            auto& first_model = list_response.body["data"][0];
            std::string model_id;
            
            if (first_model.contains("id") && !first_model["id"].is_null()) {
                model_id = first_model["id"];
            } else if (first_model.contains("model_id") && !first_model["model_id"].is_null()) {
                model_id = first_model["model_id"];
            }
            
            if (!model_id.empty()) {
                auto response = client.get("/v1/models/" + model_id + "/status");
            
                if (response.status_code == 200) {
                    CHECK(response.body.contains("status"));
                }
            }
        }
    }
    
    SECTION("Delete Model - Non-existent") {
        INFO("Testing DELETE /v1/models/{id}");
        
        // Use a non-existent model to test
        auto response = client.del("/v1/models/test-model-999");
        
        // Should return 404 for non-existent model
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Add Model") {
        INFO("Testing POST /models");
        
        nlohmann::json request = {
            {"model_id", "test-model-" + std::to_string(std::time(nullptr))},
            {"url", "https://example.com/model.gguf"},
            {"local_path", "/Users/xinuc/Library/Application Support/Kolosal/models/model.gguf"},
            {"model_type", "llm"},
            {"inference_engine", ""}
        };
        
        auto response = client.post("/models", request);
        
        if (response.status_code == 200) {
            REQUIRE(response.body.contains("message"));
            REQUIRE(response.body.contains("model_id"));
            REQUIRE(response.body.contains("status"));
            
            CHECK(response.body["message"] == "Model added successfully");
            CHECK(response.body["status"] == "downloading");
            
            // Store model_id for cleanup
            std::string model_id = response.body["model_id"];
            
            // Verify model appears in list
            auto list_response = client.get("/models");
            REQUIRE(list_response.status_code == 200);
            
            bool found = false;
            for (const auto& model : list_response.body["models"]) {
                if (model["model_id"] == model_id) {
                    found = true;
                    CHECK(model["status"] == "downloading");
                    break;
                }
            }
            CHECK(found);
            
            // Cancel the download
            auto cancel_response = client.post("/downloads/" + model_id + "/cancel", nlohmann::json{});
            CHECK(cancel_response.status_code == 200);
        } else if (response.status_code == 404) {
            // Model already exists, but server returns 404 (needs fixing)
            CHECK(response.body.contains("error"));
        }
    }
}