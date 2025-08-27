#include "kolosal/routes/retrieval/parse_document_route.hpp"
#include "kolosal/controllers/parse_document_controller.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{
    // Thread-local storage for current path
    thread_local std::string ParseDocumentRoute::current_path_;


    bool ParseDocumentRoute::match(const std::string &method, const std::string &path)
    {
        bool matches = method == "POST" && 
                      (path == "/parse_pdf" || path == "/parse_docx" || path == "/parse_html");
        
        if (matches)
        {
            // Store the path in thread-local storage (thread-safe)
            current_path_ = path;
        }
        
        return matches;
    }

    ParseDocumentRoute::DocumentType ParseDocumentRoute::getDocumentType(const std::string &path)
    {
        if (path == "/parse_pdf") return DocumentType::PDF;
        if (path == "/parse_docx") return DocumentType::DOCX;
        if (path == "/parse_html") return DocumentType::HTML;
        throw std::invalid_argument("Unknown document type for path: " + path);
    }

    std::string ParseDocumentRoute::getDataKey(DocumentType type)
    {
        switch (type)
        {
            case DocumentType::PDF: return "data";      // Match original PDF route
            case DocumentType::DOCX: return "data";     // Match original DOCX route  
            case DocumentType::HTML: return "html";     // Match original HTML route
        }
        return "";
    }

    std::string ParseDocumentRoute::getLogPrefix(DocumentType type)
    {
        switch (type)
        {
            case DocumentType::PDF: return "PDF";
            case DocumentType::DOCX: return "DOCX";
            case DocumentType::HTML: return "HTML";
        }
        return "";
    }

    void ParseDocumentRoute::sendJsonResponse(SocketType sock, const nlohmann::json &response, int status_code)
    {
        std::string response_str = response.dump();
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, status_code, response_str, headers);
    }

    void ParseDocumentRoute::sendOptionsResponse(SocketType sock, const std::string &endpoint_name, const std::string &description)
    {
        json response = {
            {"message", endpoint_name + " endpoint ready"},
            {"methods", {"POST"}},
            {"description", description}
        };
        sendJsonResponse(sock, response, 200);
    }




    void ParseDocumentRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Get document type from thread-local storage
            DocumentType docType = getDocumentType(current_path_);
            std::string log_prefix = getLogPrefix(docType);

            ServerLogger::logInfo("[Thread %u] Received %s parse request", std::this_thread::get_id(), log_prefix.c_str());

            // Handle OPTIONS request for CORS (empty body indicates OPTIONS)
            if (body.empty())
            {
                std::string description;
                switch (docType)
                {
                    case DocumentType::PDF:
                        description = "Send base64-encoded PDF data to parse text";
                        break;
                    case DocumentType::DOCX:
                        description = "Send base64-encoded DOCX data to parse text";
                        break;
                    case DocumentType::HTML:
                        description = "Send HTML content to convert to Markdown";
                        break;
                }
                sendOptionsResponse(sock, log_prefix, description);
                return;
            }

            // Create controller and process based on document type
            controllers::ParseDocumentController controller;
            controllers::BaseController::Response response;
            
            switch (docType)
            {
                case DocumentType::PDF:
                    response = controller.parsePDF(body);
                    break;
                case DocumentType::DOCX:
                    response = controller.parseDOCX(body);
                    break;
                case DocumentType::HTML:
                    response = controller.parseHTML(body);
                    break;
            }
            
            // Add CORS headers
            std::map<std::string, std::string> headers = response.headers;
            headers["Access-Control-Allow-Origin"] = "*";
            headers["Access-Control-Allow-Methods"] = "POST, OPTIONS";
            headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
            
            send_response(sock, response.status_code, response.body.dump(), headers);
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Exception in document parsing: %s", std::this_thread::get_id(), ex.what());
            json error_response;
            error_response["success"] = false;
            error_response["error"] = "Internal server error";
            error_response["details"] = ex.what();
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
