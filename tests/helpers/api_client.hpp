#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <optional>
#include <chrono>

namespace kolosal {
namespace test {

class ApiClient {
public:
    struct Response {
        int status_code;
        nlohmann::json body;
        httplib::Headers headers;
        std::string raw_body;
        std::chrono::milliseconds latency;
    };

    ApiClient(const std::string& host = "localhost", int port = 3000);
    ~ApiClient();

    // Configuration
    void setApiKey(const std::string& key);
    void setTimeout(int seconds);
    void setDebug(bool debug);

    // Auth Config endpoints
    Response getAuthConfig();
    Response updateAuthConfig(const nlohmann::json& config);
    Response getAuthStats();
    Response clearRateLimit(const std::optional<std::string>& client_ip = std::nullopt);

    // Models endpoints
    Response listModels(bool openai_format = false);
    Response addModel(const nlohmann::json& request);
    Response getModel(const std::string& model_id);
    Response removeModel(const std::string& model_id);
    Response getModelStatus(const std::string& model_id);

    // Downloads endpoints
    Response getAllDownloads();
    Response getDownloadProgress(const std::string& model_id);
    Response cancelDownload(const std::string& model_id);
    Response pauseDownload(const std::string& model_id);
    Response resumeDownload(const std::string& model_id);
    Response cancelAllDownloads();

    // Completions endpoints
    Response createCompletion(const nlohmann::json& request, bool stream = false);
    Response createChatCompletion(const nlohmann::json& request, bool stream = false);
    Response createOpenAICompletion(const nlohmann::json& request, bool stream = false);
    Response createOpenAIChatCompletion(const nlohmann::json& request, bool stream = false);

    // Embeddings endpoints
    Response createEmbedding(const nlohmann::json& request);
    Response createOpenAIEmbedding(const nlohmann::json& request);

    // Documents endpoints
    Response addDocuments(const nlohmann::json& request);
    Response removeDocuments(const nlohmann::json& request);
    Response listDocuments();
    Response getDocumentsInfo(const nlohmann::json& request);
    Response retrieveDocuments(const nlohmann::json& request);

    // Server Config endpoints
    Response getServerConfig();
    Response updateServerConfig(const nlohmann::json& config);
    Response getServerStats();
    Response saveConfig();

    // Engines endpoints
    Response listEngines();
    Response loadEngine(const std::string& engine_id);
    Response unloadEngine(const std::string& engine_id);

    // Chunking endpoints
    Response chunkText(const nlohmann::json& request);
    Response chunkFile(const std::string& file_path, const nlohmann::json& options);

    // Generic HTTP methods
    Response get(const std::string& path);
    Response post(const std::string& path, const nlohmann::json& body);
    Response post(const std::string& path, const std::string& body);
    Response put(const std::string& path, const nlohmann::json& body);
    Response del(const std::string& path);
    Response patch(const std::string& path, const nlohmann::json& body);

    // SSE streaming support
    using SSECallback = std::function<void(const std::string& data)>;
    void streamSSE(const std::string& path, const nlohmann::json& body, SSECallback callback);

private:
    std::unique_ptr<httplib::Client> client_;
    std::string api_key_;
    bool debug_;
    int timeout_seconds_;

    Response makeRequest(
        const std::string& method,
        const std::string& path,
        const std::string& body = "",
        const httplib::Headers& headers = {}
    );

    httplib::Headers getHeaders() const;
};

} // namespace test
} // namespace kolosal