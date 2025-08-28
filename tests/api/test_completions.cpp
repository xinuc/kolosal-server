#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"

using namespace kolosal::test;

TEST_CASE("Completions API", "[completions][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("Standard Completion") {
        INFO("Testing POST /completions");
        
        nlohmann::json request = {
            {"prompt", "Hello, how are"},
            {"max_tokens", 10},
            {"temperature", 0.7}
        };
        
        auto response = client.post("/completions", request);
        
        if (response.status_code == 404) {
            // Model not found - expected if no default model is set
            WARN("No model loaded for completions");
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
                CHECK(response.body["error"].contains("type"));
            }
        } else if (response.status_code == 200) {
            REQUIRE(response.body.contains("text"));
            REQUIRE(response.body.contains("tokens"));
            REQUIRE(response.body.contains("tps"));
            REQUIRE(response.body.contains("ttft"));
            
            CHECK(response.body["text"].is_string());
            CHECK(response.body["tokens"].is_number());
            CHECK(response.body["tps"].is_number());
            CHECK(response.body["ttft"].is_number());
        }
    }
    
    SECTION("Chat Completion") {
        INFO("Testing POST /chat/completions");
        
        nlohmann::json request = {
            {"model", "gpt-3.5-turbo"},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Hello"}}
            })}
        };
        
        auto response = client.post("/chat/completions", request);
        
        if (response.status_code == 404) {
            // Model not found is OK
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
            }
        } else if (response.status_code == 200) {
            CHECK(response.body.contains("choices"));
        }
    }
    
    SECTION("OpenAI Completion") {
        INFO("Testing POST /v1/completions");
        
        nlohmann::json request = {
            {"model", "text-davinci-003"},
            {"prompt", "Hello"},
            {"max_tokens", 10}
        };
        
        auto response = client.post("/v1/completions", request);
        
        if (response.status_code == 404) {
            // Model not found - expected if model doesn't exist
            WARN("Model not found for OpenAI completions");
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
            }
        } else if (response.status_code == 200) {
            // OpenAI format response
            REQUIRE(response.body.contains("id"));
            REQUIRE(response.body.contains("object"));
            REQUIRE(response.body.contains("created"));
            REQUIRE(response.body.contains("model"));
            REQUIRE(response.body.contains("choices"));
            REQUIRE(response.body.contains("usage"));
            
            CHECK(response.body["object"] == "text_completion");
            CHECK(response.body["choices"].is_array());
            
            if (response.body["choices"].size() > 0) {
                const auto& choice = response.body["choices"][0];
                CHECK(choice.contains("text"));
                CHECK(choice.contains("index"));
                CHECK(choice.contains("finish_reason"));
            }
        }
    }
    
    SECTION("OpenAI Chat Completion") {
        INFO("Testing POST /v1/chat/completions");
        
        nlohmann::json request = {
            {"model", "gpt-3.5-turbo"},
            {"messages", nlohmann::json::array({
                {{"role", "system"}, {"content", "You are a helpful assistant"}},
                {{"role", "user"}, {"content", "Hello"}}
            })},
            {"max_tokens", 50},
            {"temperature", 0.7}
        };
        
        auto response = client.post("/v1/chat/completions", request);
        
        if (response.status_code == 404) {
            // Model not found - expected if model doesn't exist
            WARN("Model not found for OpenAI chat completions");
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
                CHECK(response.body["error"].contains("type"));
            }
        } else if (response.status_code == 200) {
            // OpenAI chat format response
            REQUIRE(response.body.contains("id"));
            REQUIRE(response.body.contains("object"));
            REQUIRE(response.body.contains("created"));
            REQUIRE(response.body.contains("model"));
            REQUIRE(response.body.contains("choices"));
            REQUIRE(response.body.contains("usage"));
            
            CHECK(response.body["object"] == "chat.completion");
            CHECK(response.body["choices"].is_array());
            
            if (response.body["choices"].size() > 0) {
                const auto& choice = response.body["choices"][0];
                CHECK(choice.contains("message"));
                CHECK(choice.contains("index"));
                CHECK(choice.contains("finish_reason"));
                
                if (choice.contains("message")) {
                    CHECK(choice["message"].contains("role"));
                    CHECK(choice["message"].contains("content"));
                }
            }
        }
    }
    
    SECTION("Inference Completion") {
        INFO("Testing POST /v1/inference/completions");
        
        nlohmann::json request = {
            {"prompt", "Hello"},
            {"max_tokens", 10}
        };
        
        auto response = client.post("/v1/inference/completions", request);
        
        if (response.status_code == 404) {
            // Model not found is OK
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
            }
        } else if (response.status_code == 200) {
            CHECK(response.body.contains("text"));
        }
    }
    
    SECTION("Inference Chat Completion") {
        INFO("Testing POST /v1/inference/chat/completions");
        
        nlohmann::json request = {
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Hello"}}
            })},
            {"max_tokens", 10}
        };
        
        auto response = client.post("/v1/inference/chat/completions", request);
        
        if (response.status_code == 404) {
            // Model not found is OK
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
            }
        } else if (response.status_code == 200) {
            CHECK(response.body.contains("text"));
        }
    }
}