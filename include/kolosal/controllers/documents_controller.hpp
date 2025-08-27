#ifndef KOLOSAL_CONTROLLERS_DOCUMENTS_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_DOCUMENTS_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/retrieval/add_document_types.hpp"
#include "kolosal/retrieval/remove_document_types.hpp"
#include "kolosal/retrieval/document_list_types.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include <memory>
#include <mutex>
#include <atomic>

namespace kolosal {
namespace controllers {

class DocumentsController : public BaseController {
public:
    explicit DocumentsController(const DatabaseConfig* db_config = nullptr);
    ~DocumentsController();

    Response addDocuments(const std::string& body);
    Response addDocuments(const nlohmann::json& request);
    
    Response removeDocuments(const std::string& body);
    Response removeDocuments(const nlohmann::json& request);
    
    Response listDocuments();
    
    Response getDocumentsInfo(const std::string& body);
    Response getDocumentsInfo(const nlohmann::json& request);
    
    Response retrieveDocuments(const std::string& body);
    Response retrieveDocuments(const nlohmann::json& request);

private:
    bool ensureDocumentService();
    std::string generateRequestId(const std::string& prefix);
    
    const DatabaseConfig* db_config_;
    std::unique_ptr<kolosal::retrieval::DocumentService> document_service_;
    std::mutex service_mutex_;
    static std::atomic<long long> request_counter_;
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_DOCUMENTS_CONTROLLER_HPP