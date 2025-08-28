#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Documents API - Priority 3 Compatibility", "[documents][priority3][compatibility]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(5);
    
    SECTION("3.1.1 Add Documents - Response Format") {
        INFO("Testing POST /documents/add response format");
        
        nlohmann::json request = {
            {"documents", nlohmann::json::array({
                {
                    {"id", "doc1"},
                    {"text", "Sample document text"},
                    {"metadata", {{"author", "test"}, {"category", "sample"}}}
                }
            })},
            {"collection", "default"}
        };
        
        // Server uses /add_documents not /documents/add
        auto response = client.post("/add_documents", request);
        
        if (response.status_code == 404) {
            WARN("Documents add endpoint not implemented yet");
            SKIP("Endpoint not available");
        }
        
        if (response.status_code == 200) {
            // CRITICAL: Must be "results" not "document_ids"
            REQUIRE(response.body.contains("results"));
            REQUIRE_FALSE(response.body.contains("document_ids"));
            REQUIRE(response.body["results"].is_array());
            
            // Check results array structure
            if (response.body["results"].size() > 0) {
                const auto& result = response.body["results"][0];
                
                REQUIRE(result.contains("id"));
                REQUIRE(result.contains("success"));
                
                CHECK(result["id"].is_string());
                CHECK(result["success"].is_boolean());
                
                // May have error or message
                if (result.contains("error")) {
                    CHECK(result["error"].is_string());
                }
                if (result.contains("message")) {
                    CHECK(result["message"].is_string());
                }
            }
            
            // Check summary fields
            if (response.body.contains("successful_count")) {
                CHECK(response.body["successful_count"].is_number());
            }
            if (response.body.contains("failed_count")) {
                CHECK(response.body["failed_count"].is_number());
            }
            if (response.body.contains("collection_name")) {
                CHECK(response.body["collection_name"].is_string());
            }
        }
    }
    
    SECTION("3.1.2 Remove Documents - Response Format") {
        INFO("Testing POST /documents/remove response format");
        
        nlohmann::json request = {
            {"ids", nlohmann::json::array({"doc1", "doc2"})}
        };
        
        // Server uses /remove_documents not /documents/remove
        auto response = client.post("/remove_documents", request);
        
        if (response.status_code == 404) {
            WARN("Documents remove endpoint not implemented yet");
            SKIP("Endpoint not available");
        }
        
        if (response.status_code == 200) {
            // Check required fields
            REQUIRE(response.body.contains("removed_count"));
            REQUIRE(response.body.contains("failed_count"));
            REQUIRE(response.body.contains("not_found_count"));
            
            CHECK(response.body["removed_count"].is_number());
            CHECK(response.body["failed_count"].is_number());
            CHECK(response.body["not_found_count"].is_number());
            
            // Check details structure if present
            if (response.body.contains("details")) {
                const auto& details = response.body["details"];
                
                if (details.contains("removed")) {
                    CHECK(details["removed"].is_array());
                }
                if (details.contains("not_found")) {
                    CHECK(details["not_found"].is_array());
                }
                if (details.contains("failed")) {
                    CHECK(details["failed"].is_array());
                }
            }
        }
    }
    
    SECTION("3.1.3 List Documents") {
        INFO("Testing GET /documents response format");
        
        // Server uses /list_documents not /documents
        auto response = client.get("/list_documents");
        
        if (response.status_code == 404) {
            WARN("Documents list endpoint not implemented yet");
            SKIP("Endpoint not available");
        }
        
        if (response.status_code == 200) {
            // Check for document_ids array (actual response format)
            REQUIRE(response.body.contains("document_ids"));
            REQUIRE(response.body["document_ids"].is_array());
            
            // Check document ID structure if documents exist
            if (response.body["document_ids"].size() > 0) {
                CHECK(response.body["document_ids"][0].is_string());
            }
            
            // Check for summary/count
            if (response.body.contains("total_count")) {
                CHECK(response.body["total_count"].is_number());
            }
            if (response.body.contains("collections")) {
                CHECK(response.body["collections"].is_array());
            }
        }
    }
    
    SECTION("3.1.4 Get Document Info") {
        INFO("Testing POST /documents/info response format");
        
        nlohmann::json request = {
            {"ids", nlohmann::json::array({"doc1"})}
        };
        
        // Server uses /info_documents not /documents/info
        auto response = client.post("/info_documents", request);
        
        if (response.status_code == 404) {
            WARN("Documents info endpoint not implemented yet");
            SKIP("Endpoint not available");
        }
        
        if (response.status_code == 200) {
            // Check for documents array
            REQUIRE(response.body.contains("documents"));
            REQUIRE(response.body["documents"].is_array());
            
            if (response.body["documents"].size() > 0) {
                const auto& doc = response.body["documents"][0];
                
                REQUIRE(doc.contains("id"));
                CHECK(doc.contains("exists"));
                
                if (doc["exists"] == true) {
                    CHECK(doc.contains("metadata"));
                    CHECK(doc.contains("text_preview"));
                    CHECK(doc.contains("collection"));
                    CHECK(doc.contains("created_at"));
                }
            }
        }
    }
    
    SECTION("3.1.5 Retrieve Documents") {
        INFO("Testing POST /documents/retrieve response format");
        
        nlohmann::json request = {
            {"query", "sample query text"},
            {"limit", 5},
            {"collection", "default"}
        };
        
        // Server uses /retrieve not /documents/retrieve
        auto response = client.post("/retrieve", request);
        
        if (response.status_code == 404) {
            WARN("Documents retrieve endpoint not implemented yet");
            SKIP("Endpoint not available");
        }
        
        if (response.status_code == 200) {
            // Check for results array
            REQUIRE(response.body.contains("results"));
            REQUIRE(response.body["results"].is_array());
            
            // Check result structure if results exist
            if (response.body["results"].size() > 0) {
                const auto& result = response.body["results"][0];
                
                REQUIRE(result.contains("id"));
                REQUIRE(result.contains("text"));
                REQUIRE(result.contains("score"));
                
                CHECK(result["id"].is_string());
                CHECK(result["text"].is_string());
                CHECK(result["score"].is_number());
                
                // Check optional fields
                if (result.contains("metadata")) {
                    CHECK(result["metadata"].is_object());
                }
                if (result.contains("distance")) {
                    CHECK(result["distance"].is_number());
                }
            }
            
            // Check metadata
            if (response.body.contains("metadata")) {
                const auto& metadata = response.body["metadata"];
                
                if (metadata.contains("total_results")) {
                    CHECK(metadata["total_results"].is_number());
                }
                if (metadata.contains("query_time")) {
                    CHECK(metadata["query_time"].is_number());
                }
                if (metadata.contains("collection")) {
                    CHECK(metadata["collection"].is_string());
                }
            }
        }
    }
}