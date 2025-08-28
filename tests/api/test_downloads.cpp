#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"

using namespace kolosal::test;

TEST_CASE("Downloads API", "[downloads][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("List Downloads - Standard") {
        INFO("Testing GET /downloads");
        
        auto response = client.get("/downloads");
        
        REQUIRE(response.status_code == 200);
        REQUIRE(response.body.contains("active_downloads"));
        REQUIRE_FALSE(response.body.contains("downloads"));  // Should NOT have plain "downloads"
        REQUIRE(response.body["active_downloads"].is_array());
        
        // Check for summary
        if (response.body.contains("summary")) {
            const auto& summary = response.body["summary"];
            REQUIRE(summary.contains("total_active"));
            REQUIRE(summary.contains("llm_model_downloads"));
            REQUIRE(summary.contains("embedding_model_downloads"));
            CHECK(summary["total_active"].is_number());
            CHECK(summary["llm_model_downloads"].is_number());
            CHECK(summary["embedding_model_downloads"].is_number());
        }
        
        // Check download object structure if downloads exist
        if (response.body["active_downloads"].size() > 0) {
            const auto& first_download = response.body["active_downloads"][0];
            
            // CRITICAL: Must use "model_id" not "download_id"
            REQUIRE(first_download.contains("model_id"));
            REQUIRE_FALSE(first_download.contains("download_id"));  // Should NOT have plain "download_id"
            
            // Check other required fields
            REQUIRE(first_download.contains("status"));
            REQUIRE(first_download.contains("progress"));
            
            CHECK((first_download["status"] == "downloading" || 
                   first_download["status"] == "paused" || 
                   first_download["status"] == "failed"));
            
            const auto& progress = first_download["progress"];
            REQUIRE(progress.contains("percentage"));
            REQUIRE(progress.contains("downloaded_bytes"));
            REQUIRE(progress.contains("total_bytes"));
            CHECK(progress["percentage"].is_number());
            CHECK(progress["downloaded_bytes"].is_number());
            CHECK(progress["total_bytes"].is_number());
        }
    }
    
    SECTION("List Downloads - V1 Endpoint") {
        INFO("Testing GET /v1/downloads");
        
        auto response = client.get("/v1/downloads");
        
        REQUIRE(response.status_code == 200);
        CHECK(response.body.contains("active_downloads"));
    }
    
    SECTION("Get Download Progress") {
        INFO("Testing GET /downloads/{id}");
        
        // Get list first to find an active download
        auto list_response = client.get("/downloads");
        if (list_response.status_code == 200 && list_response.body["active_downloads"].size() > 0) {
            std::string model_id = list_response.body["active_downloads"][0]["model_id"];
            
            auto response = client.get("/downloads/" + model_id);
            
            REQUIRE(response.status_code == 200);
            REQUIRE(response.body.contains("model_id"));
            REQUIRE(response.body.contains("status"));
            REQUIRE(response.body.contains("progress"));
            
            CHECK(response.body["model_id"] == model_id);
        }
    }
    
    SECTION("Get Download Progress - V1 Endpoint") {
        INFO("Testing GET /v1/downloads/{id}");
        
        auto response = client.get("/v1/downloads/test-model");
        
        // Should return 404 for non-existent download
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Cancel Download") {
        INFO("Testing POST /downloads/{id}/cancel");
        
        // Get list first to find an active download
        auto list_response = client.get("/downloads");
        if (list_response.status_code == 200 && list_response.body["active_downloads"].size() > 0) {
            std::string model_id = list_response.body["active_downloads"][0]["model_id"];
            
            auto response = client.post("/downloads/" + model_id + "/cancel", nlohmann::json{});
            
            REQUIRE(response.status_code == 200);
            REQUIRE(response.body.contains("message"));
            CHECK(response.body.contains("status"));
            CHECK(response.body["message"].get<std::string>().find("cancelled") != std::string::npos);
        }
    }
    
    SECTION("Cancel Download - V1 Endpoint") {
        INFO("Testing POST /v1/downloads/{id}/cancel");
        
        auto response = client.post("/v1/downloads/test-model/cancel", nlohmann::json{});
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Pause Download") {
        INFO("Testing POST /downloads/{id}/pause");
        
        auto response = client.post("/downloads/test-model/pause", nlohmann::json{});
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Pause Download - V1 Endpoint") {
        INFO("Testing POST /v1/downloads/{id}/pause");
        
        auto response = client.post("/v1/downloads/test-model/pause", nlohmann::json{});
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Resume Download") {
        INFO("Testing POST /downloads/{id}/resume");
        
        auto response = client.post("/downloads/test-model/resume", nlohmann::json{});
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Resume Download - V1 Endpoint") {
        INFO("Testing POST /v1/downloads/{id}/resume");
        
        auto response = client.post("/v1/downloads/test-model/resume", nlohmann::json{});
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
    
    SECTION("Cancel All Downloads - DELETE") {
        INFO("Testing DELETE /downloads");
        
        auto response = client.del("/downloads");
        
        if (response.status_code == 200) {
            // DELETE /downloads returns download list (same as GET)
            CHECK((response.body.contains("active_downloads") || response.body.contains("summary")));
        }
    }
    
    SECTION("Cancel All Downloads - DELETE V1") {
        INFO("Testing DELETE /v1/downloads");
        
        auto response = client.del("/v1/downloads");
        
        if (response.status_code == 200) {
            // DELETE /v1/downloads returns download list (same as GET)
            CHECK((response.body.contains("active_downloads") || response.body.contains("summary")));
        }
    }
    
    SECTION("Cancel All Downloads - POST") {
        INFO("Testing POST /downloads");
        
        nlohmann::json request = {{"action", "cancel_all"}};
        auto response = client.post("/downloads", request);
        
        if (response.status_code == 200) {
            CHECK((response.body.contains("message") || response.body.contains("status")));
        }
    }
    
    SECTION("Cancel All Downloads - POST V1") {
        INFO("Testing POST /v1/downloads");
        
        nlohmann::json request = {{"action", "cancel_all"}};
        auto response = client.post("/v1/downloads", request);
        
        if (response.status_code == 200) {
            CHECK((response.body.contains("message") || response.body.contains("status")));
        }
    }
    
    SECTION("Cancel All Downloads - Dedicated Endpoint") {
        INFO("Testing POST /downloads/cancel");
        
        auto response = client.post("/downloads/cancel", nlohmann::json{});
        
        if (response.status_code == 200) {
            CHECK((response.body.contains("message") || response.body.contains("status")));
        }
    }
    
    SECTION("Cancel All Downloads - Dedicated V1 Endpoint") {
        INFO("Testing POST /v1/downloads/cancel");
        
        auto response = client.post("/v1/downloads/cancel", nlohmann::json{});
        
        if (response.status_code == 200) {
            CHECK((response.body.contains("message") || response.body.contains("status")));
        }
    }
    
    SECTION("Delete Download") {
        INFO("Testing DELETE /v1/downloads/{id}");
        
        auto response = client.del("/v1/downloads/test-model");
        
        CHECK((response.status_code == 404 || response.status_code == 200));
    }
}