#include "kolosal/controllers/parse_document_controller.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include "kolosal/retrieval/parse_docx.hpp"
#include "kolosal/retrieval/parse_html.hpp"
#include "kolosal/logger.hpp"
#include "base64.hpp"
#include <algorithm>

namespace kolosal {
namespace controllers {

BaseController::Response ParseDocumentController::parsePDF(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return parsePDF(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing PDF parse: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ParseDocumentController::parsePDF(const nlohmann::json& request) {
    try {
        // Validate request
        if (!request.contains("data") || !request["data"].is_string()) {
            return badRequestWithDetails("Missing or invalid 'data' field", 
                                         "Expected base64-encoded document data as string");
        }
        
        std::string base64_data = request["data"];
        if (base64_data.empty()) {
            return badRequestWithDetails("Empty document data", 
                                         "Document data cannot be empty");
        }
        
        // Decode base64
        std::vector<unsigned char> pdf_data = decodeBase64(base64_data);
        if (pdf_data.empty()) {
            return badRequest("Failed to decode base64 data");
        }
        
        // Parse options
        std::string method = request.value("method", "fast");
        std::string language = request.value("language", "eng");
        bool show_progress = request.value("progress", false);
        
        ServerLogger::logInfo("Parsing PDF (size: %zu bytes) using method: %s, language: %s", 
                              pdf_data.size(), method.c_str(), language.c_str());
        
        // Process PDF
        nlohmann::json result = processPDF(pdf_data, method, language, show_progress);
        
        if (result["success"]) {
            return ok(result);
        } else {
            return serverError(result["error"]);
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error parsing PDF: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ParseDocumentController::parseDOCX(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return parseDOCX(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing DOCX parse: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ParseDocumentController::parseDOCX(const nlohmann::json& request) {
    try {
        // Validate request
        if (!request.contains("data") || !request["data"].is_string()) {
            return badRequestWithDetails("Missing or invalid 'data' field", 
                                         "Expected base64-encoded DOCX data as string");
        }
        
        std::string base64_data = request["data"];
        if (base64_data.empty()) {
            return badRequestWithDetails("Empty document data", 
                                         "Document data cannot be empty");
        }
        
        // Decode base64
        std::vector<unsigned char> docx_data = decodeBase64(base64_data);
        if (docx_data.empty()) {
            return badRequest("Failed to decode base64 data");
        }
        
        ServerLogger::logInfo("Parsing DOCX (size: %zu bytes)", docx_data.size());
        
        // Process DOCX
        nlohmann::json result = processDOCX(docx_data);
        
        if (result["success"]) {
            return ok(result);
        } else {
            return serverError(result["error"]);
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error parsing DOCX: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ParseDocumentController::parseHTML(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return parseHTML(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing HTML parse: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ParseDocumentController::parseHTML(const nlohmann::json& request) {
    try {
        // Validate request
        if (!request.contains("html") || !request["html"].is_string()) {
            return badRequestWithDetails("Missing or invalid 'html' field", 
                                         "Expected HTML content as string");
        }
        
        std::string html_content = request["html"];
        if (html_content.empty()) {
            return badRequestWithDetails("Empty HTML content", 
                                         "HTML content cannot be empty");
        }
        
        ServerLogger::logDebug("Converting HTML to Markdown (length: %zu)", html_content.length());
        
        // Process HTML
        nlohmann::json result = processHTML(html_content);
        
        if (result["success"]) {
            return ok(result);
        } else {
            return serverError(result["error"]);
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error parsing HTML: %s", e.what());
        return serverError(e.what());
    }
}

std::vector<unsigned char> ParseDocumentController::decodeBase64(const std::string& base64_data) {
    try {
        std::string decoded_str = base64::decode(base64_data);
        return std::vector<unsigned char>(decoded_str.begin(), decoded_str.end());
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to decode base64: %s", e.what());
        return {};
    }
}

retrieval::PDFParseMethod ParseDocumentController::parseMethodFromString(const std::string& method_str) {
    std::string lower_method = method_str;
    std::transform(lower_method.begin(), lower_method.end(), lower_method.begin(), ::tolower);
    
    if (lower_method == "fast") {
        return retrieval::PDFParseMethod::Fast;
    } else if (lower_method == "ocr") {
        return retrieval::PDFParseMethod::OCR;
    } else if (lower_method == "visual") {
        return retrieval::PDFParseMethod::Visual;
    }
    
    return retrieval::PDFParseMethod::Fast; // Default
}

nlohmann::json ParseDocumentController::processPDF(const std::vector<unsigned char>& data,
                                                   const std::string& method,
                                                   const std::string& language,
                                                   bool show_progress) {
    nlohmann::json response;
    response["success"] = false;
    
    try {
        retrieval::PDFParseMethod parse_method = parseMethodFromString(method);
        
        // Progress callback (optional)
        retrieval::ProgressCallback progress_cb = nullptr;
        if (show_progress) {
            progress_cb = [](size_t current, size_t total) {
                ServerLogger::logInfo("PDF parsing progress: %zu/%zu pages", current, total);
            };
        }
        
        auto result = retrieval::DocumentParser::parse_pdf_from_bytes(
            data.data(), data.size(), parse_method, language, progress_cb
        );
        
        if (result.success) {
            response["success"] = true;
            response["text"] = result.text;
            response["pages_processed"] = result.pages_processed;
            response["method"] = method;
            response["language"] = language;
            response["data_size_bytes"] = data.size();
            
            ServerLogger::logInfo("PDF parsing completed successfully. Pages: %zu, Text length: %zu",
                                  result.pages_processed, result.text.length());
        } else {
            response["error"] = result.error_message;
            response["pages_processed"] = result.pages_processed;
            response["method"] = method;
            response["language"] = language;
            response["data_size_bytes"] = data.size();
            ServerLogger::logError("PDF parsing failed: %s", result.error_message.c_str());
        }
    } catch (const std::exception& e) {
        response["error"] = e.what();
        ServerLogger::logError("Exception during PDF parsing: %s", e.what());
    }
    
    return response;
}

nlohmann::json ParseDocumentController::processDOCX(const std::vector<unsigned char>& data) {
    nlohmann::json response;
    response["success"] = false;
    
    try {
        std::string parsed_text = retrieval::DOCXParser::parse_docx_from_bytes(
            data.data(), data.size()
        );
        
        response["success"] = true;
        response["text"] = parsed_text;
        response["pages_processed"] = 1; // DOCX doesn't have pages like PDF
        response["data_size_bytes"] = data.size();
        
        ServerLogger::logInfo("DOCX parsing completed successfully. Text length: %zu",
                              parsed_text.length());
    } catch (const std::exception& e) {
        response["error"] = e.what();
        response["pages_processed"] = 0;
        response["data_size_bytes"] = data.size();
        ServerLogger::logError("DOCX parsing failed: %s", e.what());
    }
    
    return response;
}

nlohmann::json ParseDocumentController::processHTML(const std::string& html_content) {
    nlohmann::json response;
    response["success"] = false;
    
    try {
        retrieval::HtmlParser parser;
        retrieval::HtmlParseResult result = parser.parseHtmlSync(html_content);
        
        if (result.success) {
            response["success"] = true;
            response["markdown"] = result.markdown;
            response["elements_processed"] = result.elements_processed;
            
            ServerLogger::logInfo("Successfully converted HTML to Markdown");
        } else {
            response["error"] = result.error_message.empty() ? 
                "Failed to parse HTML content" : result.error_message;
            response["elements_processed"] = result.elements_processed;
            ServerLogger::logError("Error converting HTML to Markdown: %s",
                                   response["error"].get<std::string>().c_str());
        }
    } catch (const std::exception& e) {
        response["error"] = e.what();
        ServerLogger::logError("Exception during HTML parsing: %s", e.what());
    }
    
    return response;
}

} // namespace controllers
} // namespace kolosal