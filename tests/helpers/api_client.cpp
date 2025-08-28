#include "api_client.hpp"
#include <iostream>
#include <sstream>

namespace kolosal {
namespace test {

ApiClient::ApiClient(const std::string& host, int port)
    : client_(std::make_unique<httplib::Client>(host, port))
    , debug_(false)
    , timeout_seconds_(30) {
    client_->set_connection_timeout(timeout_seconds_);
    client_->set_read_timeout(timeout_seconds_);
    client_->set_write_timeout(timeout_seconds_);
}

ApiClient::~ApiClient() = default;

void ApiClient::setApiKey(const std::string& key) {
    api_key_ = key;
}

void ApiClient::setTimeout(int seconds) {
    timeout_seconds_ = seconds;
    client_->set_connection_timeout(seconds);
    client_->set_read_timeout(seconds);
    client_->set_write_timeout(seconds);
}

void ApiClient::setDebug(bool debug) {
    debug_ = debug;
}

// Auth Config endpoints
ApiClient::Response ApiClient::getAuthConfig() {
    return get("/auth/config");
}

ApiClient::Response ApiClient::updateAuthConfig(const nlohmann::json& config) {
    return post("/auth/config", config);
}

ApiClient::Response ApiClient::getAuthStats() {
    return get("/auth/stats");
}

ApiClient::Response ApiClient::clearRateLimit(const std::optional<std::string>& client_ip) {
    nlohmann::json body;
    if (client_ip.has_value()) {
        body["client_ip"] = client_ip.value();
    } else {
        body["clear_all"] = true;
    }
    return post("/auth/clear-rate-limit", body);
}

// Models endpoints
ApiClient::Response ApiClient::listModels(bool openai_format) {
    if (openai_format) {
        return get("/v1/models");
    }
    return get("/models");
}

ApiClient::Response ApiClient::addModel(const nlohmann::json& request) {
    return post("/models", request);
}

ApiClient::Response ApiClient::getModel(const std::string& model_id) {
    return get("/models/" + model_id);
}

ApiClient::Response ApiClient::removeModel(const std::string& model_id) {
    return del("/models/" + model_id);
}

ApiClient::Response ApiClient::getModelStatus(const std::string& model_id) {
    return get("/models/" + model_id + "/status");
}

// Downloads endpoints
ApiClient::Response ApiClient::getAllDownloads() {
    return get("/downloads");
}

ApiClient::Response ApiClient::getDownloadProgress(const std::string& model_id) {
    return get("/downloads/" + model_id);
}

ApiClient::Response ApiClient::cancelDownload(const std::string& model_id) {
    return post("/downloads/" + model_id + "/cancel", nlohmann::json::object());
}

ApiClient::Response ApiClient::pauseDownload(const std::string& model_id) {
    return post("/downloads/" + model_id + "/pause", nlohmann::json::object());
}

ApiClient::Response ApiClient::resumeDownload(const std::string& model_id) {
    return post("/downloads/" + model_id + "/resume", nlohmann::json::object());
}

ApiClient::Response ApiClient::cancelAllDownloads() {
    return post("/downloads/cancel-all", nlohmann::json::object());
}

// Completions endpoints
ApiClient::Response ApiClient::createCompletion(const nlohmann::json& request, bool stream) {
    if (stream) {
        // For streaming, we'll need special handling
        // For now, just return a placeholder
        return post("/completions", request);
    }
    return post("/completions", request);
}

ApiClient::Response ApiClient::createChatCompletion(const nlohmann::json& request, bool stream) {
    return post("/chat/completions", request);
}

ApiClient::Response ApiClient::createOpenAICompletion(const nlohmann::json& request, bool stream) {
    return post("/v1/completions", request);
}

ApiClient::Response ApiClient::createOpenAIChatCompletion(const nlohmann::json& request, bool stream) {
    return post("/v1/chat/completions", request);
}

// Embeddings endpoints
ApiClient::Response ApiClient::createEmbedding(const nlohmann::json& request) {
    return post("/embeddings", request);
}

ApiClient::Response ApiClient::createOpenAIEmbedding(const nlohmann::json& request) {
    return post("/v1/embeddings", request);
}

// Documents endpoints
ApiClient::Response ApiClient::addDocuments(const nlohmann::json& request) {
    return post("/documents", request);
}

ApiClient::Response ApiClient::removeDocuments(const nlohmann::json& request) {
    // DELETE with body
    return makeRequest("DELETE", "/documents", request.dump());
}

ApiClient::Response ApiClient::listDocuments() {
    return get("/documents");
}

ApiClient::Response ApiClient::getDocumentsInfo(const nlohmann::json& request) {
    return post("/documents/info", request);
}

ApiClient::Response ApiClient::retrieveDocuments(const nlohmann::json& request) {
    return post("/documents/retrieve", request);
}

// Server Config endpoints
ApiClient::Response ApiClient::getServerConfig() {
    return get("/config");
}

ApiClient::Response ApiClient::updateServerConfig(const nlohmann::json& config) {
    return post("/config", config);
}

ApiClient::Response ApiClient::getServerStats() {
    return get("/stats");
}

ApiClient::Response ApiClient::saveConfig() {
    return post("/config/save", nlohmann::json::object());
}

// Engines endpoints
ApiClient::Response ApiClient::listEngines() {
    return get("/engines");
}

ApiClient::Response ApiClient::loadEngine(const std::string& engine_id) {
    return post("/engines/" + engine_id + "/load", nlohmann::json::object());
}

ApiClient::Response ApiClient::unloadEngine(const std::string& engine_id) {
    return post("/engines/" + engine_id + "/unload", nlohmann::json::object());
}

// Chunking endpoints
ApiClient::Response ApiClient::chunkText(const nlohmann::json& request) {
    return post("/chunking/text", request);
}

ApiClient::Response ApiClient::chunkFile(const std::string& file_path, const nlohmann::json& options) {
    nlohmann::json request = options;
    request["file_path"] = file_path;
    return post("/chunking/file", request);
}

// Generic HTTP methods
ApiClient::Response ApiClient::get(const std::string& path) {
    return makeRequest("GET", path);
}

ApiClient::Response ApiClient::post(const std::string& path, const nlohmann::json& body) {
    return makeRequest("POST", path, body.dump());
}

ApiClient::Response ApiClient::post(const std::string& path, const std::string& body) {
    return makeRequest("POST", path, body);
}

ApiClient::Response ApiClient::put(const std::string& path, const nlohmann::json& body) {
    return makeRequest("PUT", path, body.dump());
}

ApiClient::Response ApiClient::del(const std::string& path) {
    return makeRequest("DELETE", path);
}

ApiClient::Response ApiClient::patch(const std::string& path, const nlohmann::json& body) {
    return makeRequest("PATCH", path, body.dump());
}

// SSE streaming support
void ApiClient::streamSSE(const std::string& path, const nlohmann::json& body, SSECallback callback) {
    auto headers = getHeaders();
    headers.emplace("Accept", "text/event-stream");
    
    client_->Post(path, headers, body.dump(), "application/json");
    // Note: Full SSE streaming implementation would require more complex handling
    // This is a simplified version for testing purposes
}

// Private methods
ApiClient::Response ApiClient::makeRequest(
    const std::string& method,
    const std::string& path,
    const std::string& body,
    const httplib::Headers& extra_headers) {
    
    auto start = std::chrono::steady_clock::now();
    httplib::Headers headers = getHeaders();
    for (const auto& h : extra_headers) {
        headers.emplace(h.first, h.second);
    }

    httplib::Result res;
    
    if (debug_) {
        std::cout << "Request: " << method << " " << path << std::endl;
        if (!body.empty()) {
            std::cout << "Body: " << body << std::endl;
        }
    }

    if (method == "GET") {
        res = client_->Get(path.c_str(), headers);
    } else if (method == "POST") {
        res = client_->Post(path.c_str(), headers, body, "application/json");
    } else if (method == "PUT") {
        res = client_->Put(path.c_str(), headers, body, "application/json");
    } else if (method == "DELETE") {
        if (body.empty()) {
            res = client_->Delete(path.c_str(), headers);
        } else {
            res = client_->Delete(path.c_str(), headers, body, "application/json");
        }
    } else if (method == "PATCH") {
        res = client_->Patch(path.c_str(), headers, body, "application/json");
    }

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    Response response;
    response.latency = latency;
    
    if (res) {
        response.status_code = res->status;
        response.raw_body = res->body;
        response.headers = res->headers;
        
        if (!res->body.empty()) {
            try {
                response.body = nlohmann::json::parse(res->body);
            } catch (...) {
                // Not JSON, store as string
                response.body = res->body;
            }
        }
        
        if (debug_) {
            std::cout << "Response: " << res->status << std::endl;
            std::cout << "Body: " << res->body << std::endl;
        }
    } else {
        response.status_code = -1;
        response.body = {{"error", "Request failed"}};
        
        if (debug_) {
            std::cout << "Request failed" << std::endl;
        }
    }
    
    return response;
}

httplib::Headers ApiClient::getHeaders() const {
    httplib::Headers headers;
    headers.emplace("Content-Type", "application/json");
    
    if (!api_key_.empty()) {
        headers.emplace("X-API-Key", api_key_);
        headers.emplace("Authorization", "Bearer " + api_key_);
    }
    
    return headers;
}

} // namespace test
} // namespace kolosal