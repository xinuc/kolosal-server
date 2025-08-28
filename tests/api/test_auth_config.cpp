#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"
#include "../helpers/test_server.hpp"
#include "../helpers/test_utils.hpp"

using namespace kolosal::test;

TEST_CASE("Auth Configuration API Compatibility", "[auth][api]") {
    // Start test server
    ScopedTestServer server(3001);
    ApiClient client("localhost", 3001);
    
    SECTION("GET /auth/config returns correct format") {
        auto response = client.getAuthConfig();
        
        REQUIRE(response.status_code == 200);
        ResponseValidator::checkAuthConfig(response.body);
        
        // Check specific field values
        CHECK(response.body["rate_limiter"]["enabled"].is_boolean());
        CHECK(response.body["rate_limiter"]["max_requests"].is_number());
        CHECK(response.body["rate_limiter"]["window_size"].is_number());
        
        CHECK(response.body["cors"]["enabled"].is_boolean());
        CHECK(response.body["cors"]["allowed_origins"].is_array());
        CHECK(response.body["cors"]["allowed_methods"].is_array());
        CHECK(response.body["cors"]["allowed_headers"].is_array());
        CHECK(response.body["cors"]["allow_credentials"].is_boolean());
        CHECK(response.body["cors"]["max_age"].is_number());
        
        CHECK(response.body["api_key"]["enabled"].is_boolean());
        CHECK(response.body["api_key"]["required"].is_boolean());
        CHECK(response.body["api_key"]["header_name"].is_string());
        CHECK(response.body["api_key"]["keys_count"].is_number());
    }
    
    SECTION("POST /auth/config updates configuration") {
        // Get current config
        auto get_response = client.getAuthConfig();
        REQUIRE(get_response.status_code == 200);
        
        // Update rate limiter config
        nlohmann::json update_request = {
            {"rate_limiter", {
                {"enabled", true},
                {"max_requests", 100},
                {"window_size", 60}
            }}
        };
        
        auto update_response = client.updateAuthConfig(update_request);
        REQUIRE(update_response.status_code == 200);
        
        // Verify the response format
        CHECK(update_response.body.contains("message"));
        CHECK(update_response.body.contains("status"));
        CHECK(update_response.body["status"] == "success");
        
        // Verify the config was updated
        auto verify_response = client.getAuthConfig();
        REQUIRE(verify_response.status_code == 200);
        CHECK(verify_response.body["rate_limiter"]["enabled"] == true);
        CHECK(verify_response.body["rate_limiter"]["max_requests"] == 100);
        CHECK(verify_response.body["rate_limiter"]["window_size"] == 60);
    }
    
    SECTION("GET /auth/stats returns correct format") {
        auto response = client.getAuthStats();
        
        REQUIRE(response.status_code == 200);
        ResponseValidator::checkAuthStats(response.body);
        
        // Check specific fields
        CHECK(response.body["rate_limit_stats"]["total_clients"].is_number());
        CHECK(response.body["rate_limit_stats"]["total_requests"].is_number());
        CHECK(response.body["rate_limit_stats"]["clients"].is_object());
        
        CHECK(response.body["cors_stats"].is_object());
    }
    
    SECTION("POST /auth/clear-rate-limit clears rate limits") {
        // Test clearing all rate limits
        auto clear_all_response = client.clearRateLimit();
        REQUIRE(clear_all_response.status_code == 200);
        CHECK(clear_all_response.body.contains("message"));
        CHECK(clear_all_response.body.contains("status"));
        CHECK(clear_all_response.body["status"] == "success");
        
        // Test clearing specific client
        auto clear_client_response = client.clearRateLimit("192.168.1.1");
        REQUIRE(clear_client_response.status_code == 200);
        CHECK(clear_client_response.body.contains("message"));
        CHECK(clear_client_response.body["message"].get<std::string>().find("192.168.1.1") != std::string::npos);
    }
    
    SECTION("Invalid requests return proper error format") {
        // Test with invalid JSON
        auto invalid_response = client.post("/auth/config", "invalid json");
        REQUIRE(invalid_response.status_code == 400);
        ResponseValidator::checkErrorResponse(invalid_response.body);
        
        // Test with invalid field types
        nlohmann::json invalid_request = {
            {"rate_limiter", {
                {"max_requests", "not a number"}
            }}
        };
        
        auto type_error_response = client.updateAuthConfig(invalid_request);
        REQUIRE(type_error_response.status_code == 400);
        ResponseValidator::checkErrorResponse(type_error_response.body);
    }
    
    SECTION("CORS configuration updates correctly") {
        nlohmann::json cors_update = {
            {"cors", {
                {"enabled", true},
                {"allowed_origins", {"http://localhost:3000", "http://example.com"}},
                {"allowed_methods", {"GET", "POST", "PUT", "DELETE"}},
                {"allowed_headers", {"Content-Type", "Authorization"}},
                {"allow_credentials", true},
                {"max_age", 3600}
            }}
        };
        
        auto response = client.updateAuthConfig(cors_update);
        REQUIRE(response.status_code == 200);
        
        // Verify the update
        auto verify_response = client.getAuthConfig();
        REQUIRE(verify_response.status_code == 200);
        
        const auto& cors = verify_response.body["cors"];
        CHECK(cors["enabled"] == true);
        CHECK(cors["allowed_origins"].size() == 2);
        CHECK(cors["allowed_methods"].size() == 4);
        CHECK(cors["allowed_headers"].size() == 2);
        CHECK(cors["allow_credentials"] == true);
        CHECK(cors["max_age"] == 3600);
    }
    
    SECTION("API key configuration updates correctly") {
        nlohmann::json api_key_update = {
            {"api_key", {
                {"enabled", true},
                {"required", true},
                {"header_name", "X-API-Key"},
                {"api_keys", {"test-key-1", "test-key-2", "test-key-3"}}
            }}
        };
        
        auto response = client.updateAuthConfig(api_key_update);
        REQUIRE(response.status_code == 200);
        
        // Verify the update
        auto verify_response = client.getAuthConfig();
        REQUIRE(verify_response.status_code == 200);
        
        const auto& api_key = verify_response.body["api_key"];
        CHECK(api_key["enabled"] == true);
        CHECK(api_key["required"] == true);
        CHECK(api_key["header_name"] == "X-API-Key");
        CHECK(api_key["keys_count"] == 3);
    }
}

TEST_CASE("Auth Configuration Field Compatibility", "[auth][fields]") {
    ScopedTestServer server(3001);
    ApiClient client("localhost", 3001);
    
    SECTION("Rate limit clear uses 'client_ip' field") {
        nlohmann::json request = {
            {"client_ip", "10.0.0.1"}
        };
        
        auto response = client.post("/auth/clear-rate-limit", request);
        REQUIRE(response.status_code == 200);
        
        // Verify the field name is accepted
        CHECK(response.body["message"].get<std::string>().find("10.0.0.1") != std::string::npos);
    }
    
    SECTION("Clear all rate limits uses 'clear_all' field") {
        nlohmann::json request = {
            {"clear_all", true}
        };
        
        auto response = client.post("/auth/clear-rate-limit", request);
        REQUIRE(response.status_code == 200);
        CHECK(response.body["message"].get<std::string>().find("All") != std::string::npos);
    }
}