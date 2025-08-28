#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Internet Search API", "[search][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(10); // Search may take longer
    
    SECTION("GET Internet Search - /internet_search") {
        INFO("Testing GET /internet_search endpoint");
        
        // GET with query parameter
        auto response = client.get("/internet_search?q=test+query");
        
        // May return 404 if not configured or 200 if enabled
        if (response.status_code == 404) {
            WARN("Internet search endpoint not configured");
            SKIP("Internet search not available");
        }
        
        if (response.status_code == 200) {
            // Check response structure
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
                
                if (response.body["results"].size() > 0) {
                    const auto& result = response.body["results"][0];
                    
                    if (result.contains("title")) {
                        CHECK(result["title"].is_string());
                    }
                    if (result.contains("url")) {
                        CHECK(result["url"].is_string());
                    }
                    if (result.contains("snippet")) {
                        CHECK(result["snippet"].is_string());
                    }
                }
            }
            
            if (response.body.contains("query")) {
                CHECK(response.body["query"].is_string());
            }
            if (response.body.contains("total_results")) {
                CHECK(response.body["total_results"].is_number());
            }
        }
    }
    
    SECTION("POST Internet Search - /internet_search") {
        INFO("Testing POST /internet_search endpoint");
        
        nlohmann::json request = {
            {"query", "test search query"},
            {"max_results", 5}
        };
        
        auto response = client.post("/internet_search", request);
        
        if (response.status_code == 404) {
            WARN("Internet search endpoint not configured");
            SKIP("Internet search not available");
        }
        
        if (response.status_code == 200) {
            // Check response structure
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
            }
        }
    }
    
    SECTION("GET Internet Search - /v1/internet_search") {
        INFO("Testing GET /v1/internet_search endpoint");
        
        auto response = client.get("/v1/internet_search?q=test");
        
        if (response.status_code == 404) {
            WARN("Internet search endpoint not configured");
            SKIP("Internet search not available");
        }
        
        if (response.status_code == 200) {
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
            }
        }
    }
    
    SECTION("POST Internet Search - /v1/internet_search") {
        INFO("Testing POST /v1/internet_search endpoint");
        
        nlohmann::json request = {
            {"query", "test"},
            {"max_results", 3}
        };
        
        auto response = client.post("/v1/internet_search", request);
        
        if (response.status_code == 404) {
            WARN("Internet search endpoint not configured");
            SKIP("Internet search not available");
        }
        
        if (response.status_code == 200) {
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
            }
        }
    }
    
    SECTION("GET Search - /search") {
        INFO("Testing GET /search endpoint");
        
        auto response = client.get("/search?q=test");
        
        if (response.status_code == 404) {
            WARN("Search endpoint not configured");
            SKIP("Search not available");
        }
        
        if (response.status_code == 200) {
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
            }
        }
    }
    
    SECTION("POST Search - /search") {
        INFO("Testing POST /search endpoint");
        
        nlohmann::json request = {
            {"query", "test search"}
        };
        
        auto response = client.post("/search", request);
        
        if (response.status_code == 404) {
            WARN("Search endpoint not configured");
            SKIP("Search not available");
        }
        
        if (response.status_code == 200) {
            if (response.body.contains("results")) {
                CHECK(response.body["results"].is_array());
            }
        }
    }
}