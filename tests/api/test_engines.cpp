#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"

using namespace kolosal::test;

TEST_CASE("Engines API", "[engines][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("List Engines - Standard") {
        INFO("Testing GET /engines");
        
        auto response = client.get("/engines");
        
        REQUIRE(response.status_code == 200);
        REQUIRE(response.body.contains("inference_engines"));
        REQUIRE_FALSE(response.body.contains("engines"));  // Should NOT have plain "engines"
        REQUIRE(response.body["inference_engines"].is_array());
        
        // Check engine object structure if engines exist
        if (response.body["inference_engines"].size() > 0) {
            const auto& first_engine = response.body["inference_engines"][0];
            
            REQUIRE(first_engine.contains("name"));
            REQUIRE(first_engine.contains("library_path"));
            
            CHECK(first_engine["name"].is_string());
            CHECK(first_engine["library_path"].is_string());
            
            if (first_engine.contains("version")) {
                CHECK(first_engine["version"].is_string());
            }
            if (first_engine.contains("description")) {
                CHECK(first_engine["description"].is_string());
            }
            if (first_engine.contains("loaded")) {
                CHECK(first_engine["loaded"].is_boolean());
            }
        }
    }
    
    SECTION("List Engines - V1 Endpoint") {
        INFO("Testing GET /v1/engines");
        
        auto response = client.get("/v1/engines");
        
        REQUIRE(response.status_code == 200);
        CHECK(response.body.contains("inference_engines"));
    }
    
    SECTION("Add Engine") {
        INFO("Testing POST /engines");
        
        nlohmann::json request = {
            {"name", "test-engine"},
            {"library_path", "/path/to/engine.so"},
            {"description", "Test engine"}
        };
        
        auto response = client.post("/engines", request);
        
        if (response.status_code == 200 || response.status_code == 201) {
            CHECK(response.body.contains("message"));
        }
    }
    
    SECTION("Add Engine - V1 Endpoint") {
        INFO("Testing POST /v1/engines");
        
        nlohmann::json request = {
            {"name", "test-engine"},
            {"library_path", "/path/to/engine.so"},
            {"description", "Test engine"}
        };
        
        auto response = client.post("/v1/engines", request);
        
        if (response.status_code == 200 || response.status_code == 201) {
            CHECK(response.body.contains("message"));
        }
    }
    
    SECTION("Update/Load Engine") {
        INFO("Testing PUT /v1/engines");
        
        // Get first engine name
        auto list_response = client.get("/v1/engines");
        if (list_response.status_code == 200 && list_response.body["inference_engines"].size() > 0) {
            std::string engine_name = list_response.body["inference_engines"][0]["name"];
            
            nlohmann::json request = {
                {"engine_name", engine_name}
            };
            
            auto response = client.put("/v1/engines", request);
            
            if (response.status_code == 200) {
                CHECK(response.body.contains("message"));
            }
        }
    }
}