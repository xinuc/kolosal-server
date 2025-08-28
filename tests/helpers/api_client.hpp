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

    ApiClient(const std::string& host = "localhost", int port = 8080);
    ~ApiClient();

    // Configuration
    void setTimeout(int seconds);

    // Auth Config endpoints
    Response getAuthConfig();
    Response updateAuthConfig(const nlohmann::json& config);
    Response getAuthStats();
    Response clearRateLimit(const std::optional<std::string>& client_ip = std::nullopt);

    // Note: Unused endpoint methods have been removed.
    // Add them back as needed when writing new tests.

    // Generic HTTP methods
    Response get(const std::string& path);
    Response post(const std::string& path, const nlohmann::json& body);
    Response post(const std::string& path, const std::string& body);
    Response put(const std::string& path, const nlohmann::json& body);
    Response del(const std::string& path);


private:
    std::unique_ptr<httplib::Client> client_;
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