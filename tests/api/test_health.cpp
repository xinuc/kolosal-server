#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Health API - Priority 1 Compatibility", "[health][priority1][compatibility]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("1.5.1 Health Check - Basic") {
        INFO("Testing GET /health endpoint");
        
        auto response = client.get("/health");
        
        REQUIRE(response.status_code == 200);
        
        // Check required top-level fields
        REQUIRE(response.body.contains("status"));
        REQUIRE(response.body.contains("timestamp"));
        
        // Status should be "healthy" or similar
        CHECK(response.body["status"].is_string());
        CHECK((response.body["status"] == "healthy" || 
               response.body["status"] == "ok" || 
               response.body["status"] == "running"));
        
        // Timestamp should be a number
        CHECK(response.body["timestamp"].is_number());
    }
    
    SECTION("1.5.2 Health Check - Detailed Info") {
        INFO("Testing GET /health detailed response");
        
        auto response = client.get("/health");
        
        REQUIRE(response.status_code == 200);
        
        // Check for server info
        if (response.body.contains("server")) {
            const auto& server = response.body["server"];
            
            if (server.contains("name")) {
                CHECK(server["name"].is_string());
            }
            if (server.contains("version")) {
                CHECK(server["version"].is_string());
            }
            if (server.contains("uptime")) {
                CHECK(server["uptime"].is_string());
            }
        }
        
        // Check for engines info
        if (response.body.contains("engines")) {
            CHECK(response.body["engines"].is_array());
            
            if (response.body["engines"].size() > 0) {
                const auto& engine = response.body["engines"][0];
                
                if (engine.contains("engine_id")) {
                    CHECK(engine["engine_id"].is_string());
                }
                if (engine.contains("status")) {
                    CHECK(engine["status"].is_string());
                    CHECK((engine["status"] == "loaded" || 
                           engine["status"] == "unloaded" ||
                           engine["status"] == "loading"));
                }
            }
        }
        
        // Check for node manager info
        if (response.body.contains("node_manager")) {
            const auto& nm = response.body["node_manager"];
            
            if (nm.contains("loaded_engines")) {
                CHECK(nm["loaded_engines"].is_number());
            }
            if (nm.contains("total_engines")) {
                CHECK(nm["total_engines"].is_number());
            }
            if (nm.contains("autoscaling")) {
                CHECK(nm["autoscaling"].is_string());
            }
        }
    }
    
    SECTION("1.5.3 Status Endpoint (Alias)") {
        INFO("Testing GET /status endpoint");
        
        auto response = client.get("/status");
        
        REQUIRE(response.status_code == 200);
        
        // Should have same structure as /health
        REQUIRE(response.body.contains("status"));
        REQUIRE(response.body.contains("timestamp"));
        
        CHECK(response.body["status"].is_string());
        CHECK(response.body["timestamp"].is_number());
    }
    
    SECTION("1.5.4 V1 Health Endpoint") {
        INFO("Testing GET /v1/health endpoint");
        
        auto response = client.get("/v1/health");
        
        REQUIRE(response.status_code == 200);
        
        // Should have same structure as /health
        REQUIRE(response.body.contains("status"));
        REQUIRE(response.body.contains("timestamp"));
        
        CHECK(response.body["status"].is_string());
        CHECK(response.body["timestamp"].is_number());
    }
}