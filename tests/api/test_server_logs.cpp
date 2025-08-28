#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Server Logs API", "[logs][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("Get Server Logs - /logs") {
        INFO("Testing GET /logs endpoint");
        
        auto response = client.get("/logs");
        
        REQUIRE(response.status_code == 200);
        
        // Check required fields
        REQUIRE(response.body.contains("logs"));
        REQUIRE(response.body["logs"].is_array());
        
        // Check optional metadata fields
        if (response.body.contains("total_count")) {
            CHECK(response.body["total_count"].is_number());
        }
        if (response.body.contains("retrieved_at")) {
            CHECK((response.body["retrieved_at"].is_number() || response.body["retrieved_at"].is_string()));
        }
        
        // Check log entry structure if logs exist
        if (response.body["logs"].size() > 0) {
            const auto& log_entry = response.body["logs"][0];
            
            // Common log fields
            if (log_entry.contains("timestamp")) {
                CHECK((log_entry["timestamp"].is_number() || log_entry["timestamp"].is_string()));
            }
            if (log_entry.contains("level")) {
                CHECK(log_entry["level"].is_string());
                CHECK((log_entry["level"] == "DEBUG" || 
                       log_entry["level"] == "INFO" ||
                       log_entry["level"] == "WARNING" ||
                       log_entry["level"] == "ERROR"));
            }
            if (log_entry.contains("message")) {
                CHECK(log_entry["message"].is_string());
            }
            if (log_entry.contains("source")) {
                CHECK(log_entry["source"].is_string());
            }
        }
    }
    
    SECTION("Get Server Logs - /v1/logs") {
        INFO("Testing GET /v1/logs endpoint");
        
        auto response = client.get("/v1/logs");
        
        REQUIRE(response.status_code == 200);
        REQUIRE(response.body.contains("logs"));
        REQUIRE(response.body["logs"].is_array());
    }
    
    SECTION("Get Server Logs - /server/logs") {
        INFO("Testing GET /server/logs endpoint");
        
        auto response = client.get("/server/logs");
        
        REQUIRE(response.status_code == 200);
        REQUIRE(response.body.contains("logs"));
        REQUIRE(response.body["logs"].is_array());
    }
}