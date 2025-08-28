#include "kolosal/controllers/documents_controller.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <chrono>
#include <sstream>

namespace kolosal {
namespace controllers {

std::atomic<long long> DocumentsController::request_counter_{0};

DocumentsController::DocumentsController(const DatabaseConfig* db_config)
    : db_config_(db_config) {
    ServerLogger::logInfo("DocumentsController initialized");
}

DocumentsController::~DocumentsController() = default;

BaseController::Response DocumentsController::addDocuments(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return addDocuments(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing add documents: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::addDocuments(const nlohmann::json& j) {
    try {
        // Parse the request using the DTO model
        kolosal::retrieval::AddDocumentsRequest request;
        try {
            request.from_json(j);
        } catch (const std::runtime_error& ex) {
            return badRequest(ex.what());
        }
        
        // Validate the request
        if (!request.validate()) {
            return badRequest("Invalid request parameters");
        }
        
        // Generate unique request ID
        std::string requestId = generateRequestId("doc");
        
        ServerLogger::logInfo("Processing %zu documents for indexing (Request ID: %s)", 
                              request.documents.size(), requestId.c_str());
        
        // Initialize document service if needed
        if (!ensureDocumentService()) {
            return serverErrorWithCode("Failed to initialize document service", "service_error");
        }
        
        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected) {
            return serviceUnavailableWithCode("Database connection failed", "service_unavailable");
        }
        
        // Process documents
        ServerLogger::logDebug("Submitting documents for processing");
        auto response_future = document_service_->addDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::AddDocumentsResponse response = response_future.get();
        
        ServerLogger::logInfo("Successfully processed documents - Success: %d, Failed: %d", 
                              response.successful_count, response.failed_count);
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error adding documents: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::removeDocuments(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return removeDocuments(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing remove documents: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::removeDocuments(const nlohmann::json& j) {
    try {
        // Parse the request using the DTO model
        kolosal::retrieval::RemoveDocumentsRequest request;
        try {
            request.from_json(j);
        } catch (const std::runtime_error& ex) {
            return badRequest(ex.what());
        }
        
        // Validate the request
        if (!request.validate()) {
            return badRequest("Invalid request parameters: document_ids cannot be empty");
        }
        
        // Generate unique request ID
        std::string requestId = generateRequestId("rem");
        
        ServerLogger::logInfo("Processing removal of %zu documents (Request ID: %s)", 
                              request.ids.size(), requestId.c_str());
        
        // Initialize document service if needed
        if (!ensureDocumentService()) {
            return serverErrorWithCode("Failed to initialize document service", "service_error");
        }
        
        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected) {
            return serviceUnavailableWithCode("Database connection failed", "service_unavailable");
        }
        
        // Process removal
        ServerLogger::logDebug("Submitting documents for removal");
        auto response_future = document_service_->removeDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::RemoveDocumentsResponse response = response_future.get();
        
        ServerLogger::logInfo("Successfully processed document removal - Removed: %d, Failed: %d, Not Found: %d", 
                              response.removed_count, response.failed_count, response.not_found_count);
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error removing documents: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::listDocuments() {
    try {
        ServerLogger::logInfo("Processing list documents request");
        
        // Initialize document service if needed
        if (!ensureDocumentService()) {
            return serverErrorWithCode("Failed to initialize document service", "service_error");
        }
        
        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected) {
            return serviceUnavailableWithCode("Database connection failed", "service_unavailable");
        }
        
        // Get list of document IDs
        ServerLogger::logDebug("Fetching document list");
        auto list_future = document_service_->listDocuments();
        std::vector<std::string> document_ids = list_future.get();
        
        // Create response
        kolosal::retrieval::ListDocumentsResponse response;
        response.document_ids = std::move(document_ids);
        response.total_count = static_cast<int>(response.document_ids.size());
        response.collection_name = "documents"; // Default collection name
        
        ServerLogger::logInfo("Successfully returned list of %d documents", response.total_count);
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing documents: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::getDocumentsInfo(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return getDocumentsInfo(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing document info: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::getDocumentsInfo(const nlohmann::json& j) {
    try {
        // Parse the request using the DTO model
        kolosal::retrieval::DocumentsInfoRequest request;
        try {
            request.from_json(j);
        } catch (const std::runtime_error& ex) {
            return badRequest(ex.what());
        }
        
        // Validate the request
        if (!request.validate()) {
            return badRequest("Invalid request parameters: ids cannot be empty");
        }
        
        ServerLogger::logInfo("Processing info request for %zu documents", request.ids.size());
        
        // Initialize document service if needed
        if (!ensureDocumentService()) {
            return serverErrorWithCode("Failed to initialize document service", "service_error");
        }
        
        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected) {
            return serviceUnavailableWithCode("Database connection failed", "service_unavailable");
        }
        
        // Get documents info
        ServerLogger::logDebug("Fetching document info");
        auto info_future = document_service_->getDocumentsInfo(request.ids);
        auto document_infos = info_future.get();
        
        // Create response
        kolosal::retrieval::DocumentsInfoResponse response;
        response.collection_name = "documents"; // Default collection name
        
        for (const auto& doc_pair : document_infos) {
            const auto& id = doc_pair.first;
            const auto& info_opt = doc_pair.second;
            if (info_opt.has_value()) {
                kolosal::retrieval::DocumentInfo doc_info;
                doc_info.id = id;
                doc_info.text = info_opt.value().first;
                doc_info.metadata = info_opt.value().second;
                response.documents.push_back(std::move(doc_info));
                response.found_count++;
            } else {
                response.not_found_ids.push_back(id);
                response.not_found_count++;
            }
        }
        
        ServerLogger::logInfo("Successfully returned info for %d/%zu documents", 
                              response.found_count, request.ids.size());
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting document info: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::retrieveDocuments(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return retrieveDocuments(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error processing retrieve request: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DocumentsController::retrieveDocuments(const nlohmann::json& j) {
    try {
        // Parse the request using the DTO model
        kolosal::retrieval::RetrieveRequest request;
        try {
            request.from_json(j);
        } catch (const std::runtime_error& ex) {
            return badRequest(ex.what());
        }
        
        // Validate the request
        if (!request.validate()) {
            return badRequest("Invalid request parameters");
        }
        
        // Generate unique request ID
        std::string requestId = generateRequestId("ret");
        
        ServerLogger::logInfo("Processing retrieval for query: '%s' (k=%d, Request ID: %s)", 
                              request.query.c_str(), request.k, requestId.c_str());
        
        // Initialize document service if needed
        if (!ensureDocumentService()) {
            return serverErrorWithCode("Failed to initialize document service", "service_error");
        }
        
        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected) {
            return serviceUnavailableWithCode("Database connection failed", "service_unavailable");
        }
        
        // Process retrieval
        ServerLogger::logDebug("Submitting retrieval for processing");
        auto response_future = document_service_->retrieveDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::RetrieveResponse response = response_future.get();
        
        ServerLogger::logInfo("Successfully retrieved %d documents for query", response.total_found);
        
        return ok(response.to_json());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error retrieving documents: %s", e.what());
        return serverError(e.what());
    }
}

bool DocumentsController::ensureDocumentService() {
    std::lock_guard<std::mutex> lock(service_mutex_);
    if (!document_service_) {
        DatabaseConfig config;
        
        if (db_config_) {
            config = *db_config_;
        } else {
            // Get database config from the server configuration
            auto& serverConfig = ServerConfig::getInstance();
            config = serverConfig.database;
        }
        
        // Ensure Qdrant is configured with proper defaults if not set
        if (config.qdrant.host.empty()) {
            config.qdrant.host = "localhost";
        }
        if (config.qdrant.port == 0) {
            config.qdrant.port = 6333;
        }
        if (config.qdrant.collectionName.empty()) {
            config.qdrant.collectionName = "documents";
        }
        if (config.qdrant.defaultEmbeddingModel.empty()) {
            config.qdrant.defaultEmbeddingModel = "text-embedding-3-small";
        }
        if (config.qdrant.timeout == 0) {
            config.qdrant.timeout = 30;
        }
        if (config.qdrant.maxConnections == 0) {
            config.qdrant.maxConnections = 10;
        }
        if (config.qdrant.connectionTimeout == 0) {
            config.qdrant.connectionTimeout = 5;
        }
        if (config.qdrant.embeddingBatchSize == 0) {
            config.qdrant.embeddingBatchSize = 5;
        }
        
        document_service_ = std::make_unique<kolosal::retrieval::DocumentService>(config);
        
        // Initialize service
        bool initialized = document_service_->initialize().get();
        if (!initialized) {
            return false;
        }
        
        ServerLogger::logInfo("DocumentService initialized successfully");
    }
    return true;
}

std::string DocumentsController::generateRequestId(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "-" << ++request_counter_ << "-" 
       << std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();
    return ss.str();
}

} // namespace controllers
} // namespace kolosal