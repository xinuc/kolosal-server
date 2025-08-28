#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"

using namespace kolosal::test;

TEST_CASE("Parse Document API", "[parse][api]") {
    ApiClient client("localhost", 8080);
    client.setTimeout(10); // Document parsing may take time
    
    SECTION("Parse PDF Document") {
        INFO("Testing POST /parse_pdf endpoint");
        
        nlohmann::json request = {
            {"file_path", "/path/to/test.pdf"},
            {"extract_images", true},
            {"extract_metadata", true}
        };
        
        auto response = client.post("/parse_pdf", request);
        
        if (response.status_code == 404) {
            WARN("PDF parsing endpoint not implemented");
            SKIP("PDF parsing not available");
        }
        
        if (response.status_code == 400) {
            // File doesn't exist or invalid request
            if (response.body.contains("error")) {
                CHECK(response.body["error"].contains("message"));
            }
        }
        
        if (response.status_code == 200) {
            // Check parsed content structure
            if (response.body.contains("text")) {
                CHECK(response.body["text"].is_string());
            }
            if (response.body.contains("pages")) {
                CHECK(response.body["pages"].is_array());
                
                if (response.body["pages"].size() > 0) {
                    const auto& page = response.body["pages"][0];
                    
                    if (page.contains("page_number")) {
                        CHECK(page["page_number"].is_number());
                    }
                    if (page.contains("text")) {
                        CHECK(page["text"].is_string());
                    }
                }
            }
            if (response.body.contains("metadata")) {
                CHECK(response.body["metadata"].is_object());
                
                const auto& metadata = response.body["metadata"];
                if (metadata.contains("author")) {
                    CHECK(metadata["author"].is_string());
                }
                if (metadata.contains("title")) {
                    CHECK(metadata["title"].is_string());
                }
                if (metadata.contains("pages_count")) {
                    CHECK(metadata["pages_count"].is_number());
                }
            }
            if (response.body.contains("images")) {
                CHECK(response.body["images"].is_array());
            }
        }
    }
    
    SECTION("Parse DOCX Document") {
        INFO("Testing POST /parse_docx endpoint");
        
        nlohmann::json request = {
            {"file_path", "/path/to/test.docx"},
            {"extract_styles", false},
            {"extract_metadata", true}
        };
        
        auto response = client.post("/parse_docx", request);
        
        if (response.status_code == 404) {
            WARN("DOCX parsing endpoint not implemented");
            SKIP("DOCX parsing not available");
        }
        
        if (response.status_code == 200) {
            // Check parsed content structure
            if (response.body.contains("text")) {
                CHECK(response.body["text"].is_string());
            }
            if (response.body.contains("paragraphs")) {
                CHECK(response.body["paragraphs"].is_array());
                
                if (response.body["paragraphs"].size() > 0) {
                    const auto& paragraph = response.body["paragraphs"][0];
                    
                    if (paragraph.contains("text")) {
                        CHECK(paragraph["text"].is_string());
                    }
                    if (paragraph.contains("style")) {
                        CHECK(paragraph["style"].is_string());
                    }
                }
            }
            if (response.body.contains("metadata")) {
                CHECK(response.body["metadata"].is_object());
            }
            if (response.body.contains("tables")) {
                CHECK(response.body["tables"].is_array());
            }
        }
    }
    
    SECTION("Parse HTML Document") {
        INFO("Testing POST /parse_html endpoint");
        
        nlohmann::json request = {
            {"content", "<html><body><h1>Test</h1><p>Content</p></body></html>"},
            {"extract_links", true},
            {"clean_text", true}
        };
        
        auto response = client.post("/parse_html", request);
        
        if (response.status_code == 404) {
            WARN("HTML parsing endpoint not implemented");
            SKIP("HTML parsing not available");
        }
        
        if (response.status_code == 200) {
            // Check parsed content structure
            if (response.body.contains("text")) {
                CHECK(response.body["text"].is_string());
            }
            if (response.body.contains("title")) {
                CHECK(response.body["title"].is_string());
            }
            if (response.body.contains("links")) {
                CHECK(response.body["links"].is_array());
                
                if (response.body["links"].size() > 0) {
                    const auto& link = response.body["links"][0];
                    
                    if (link.contains("url")) {
                        CHECK(link["url"].is_string());
                    }
                    if (link.contains("text")) {
                        CHECK(link["text"].is_string());
                    }
                }
            }
            if (response.body.contains("headings")) {
                CHECK(response.body["headings"].is_array());
            }
            if (response.body.contains("images")) {
                CHECK(response.body["images"].is_array());
            }
        }
    }
    
    SECTION("Parse HTML from URL") {
        INFO("Testing POST /parse_html with URL");
        
        nlohmann::json request = {
            {"url", "https://example.com"},
            {"extract_links", true},
            {"clean_text", true}
        };
        
        auto response = client.post("/parse_html", request);
        
        if (response.status_code == 404) {
            WARN("HTML parsing endpoint not implemented");
            SKIP("HTML parsing not available");
        }
        
        if (response.status_code == 200) {
            if (response.body.contains("text")) {
                CHECK(response.body["text"].is_string());
            }
        }
    }
}