#ifndef KOLOSAL_CONTROLLERS_PARSE_DOCUMENT_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_PARSE_DOCUMENT_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include <string>
#include <vector>

namespace kolosal {
namespace controllers {

class ParseDocumentController : public BaseController {
public:
    enum class DocumentType {
        PDF,
        DOCX,
        HTML
    };

    ParseDocumentController() = default;
    ~ParseDocumentController() = default;

    // Main entry points for each document type
    Response parsePDF(const std::string& body);
    Response parsePDF(const nlohmann::json& request);
    
    Response parseDOCX(const std::string& body);
    Response parseDOCX(const nlohmann::json& request);
    
    Response parseHTML(const std::string& body);
    Response parseHTML(const nlohmann::json& request);

private:
    // Helper methods
    std::vector<unsigned char> decodeBase64(const std::string& base64_data);
    retrieval::PDFParseMethod parseMethodFromString(const std::string& method_str);
    
    // Process different document types
    nlohmann::json processPDF(const std::vector<unsigned char>& data,
                              const std::string& method = "fast",
                              const std::string& language = "eng",
                              bool show_progress = false);
                              
    nlohmann::json processDOCX(const std::vector<unsigned char>& data);
    
    nlohmann::json processHTML(const std::string& html_content);
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_PARSE_DOCUMENT_CONTROLLER_HPP