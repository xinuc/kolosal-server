#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Simple Server Connection Test", "[simple][api]") {
    // Connect to existing server on port 8080
    ApiClient client("localhost", 8080);
    client.setTimeout(5); // 5 seconds timeout
    
    SECTION("Server is responsive") {
        auto response = client.get("/models");
        
        // Server should respond (either 200 or error)
        REQUIRE(response.status_code != -1);
        
        if (response.status_code == 200) {
            INFO("Server responded successfully");
            CHECK((response.body.is_object() || response.body.is_array()));
        } else {
            INFO("Server returned status: " << response.status_code);
        }
    }
    
    SECTION("Auth config endpoint works") {
        auto response = client.get("/auth/config");
        
        // Check that we got a response
        REQUIRE(response.status_code != -1);
        
        if (response.status_code == 200) {
            CHECK(response.body.is_object());
            // Basic structure check
            if (response.body.contains("enabled")) {
                CHECK(response.body["enabled"].is_boolean());
            }
        }
    }
}