#include "api_client.hpp"
#include <iostream>
#include <sstream>

namespace kolosal {
namespace test {

ApiClient::ApiClient(const std::string& host, int port)
    : client_(std::make_unique<httplib::Client>(host, port))
    , timeout_seconds_(30) {
    client_->set_connection_timeout(timeout_seconds_);
    client_->set_read_timeout(timeout_seconds_);
    client_->set_write_timeout(timeout_seconds_);
}

ApiClient::~ApiClient() = default;

void ApiClient::setTimeout(int seconds) {
    timeout_seconds_ = seconds;
    client_->set_connection_timeout(seconds);
    client_->set_read_timeout(seconds);
    client_->set_write_timeout(seconds);
}

// Auth Config endpoints
ApiClient::Response ApiClient::getAuthConfig() {
    return get("/v1/auth/config");
}

ApiClient::Response ApiClient::updateAuthConfig(const nlohmann::json& config) {
    return put("/v1/auth/config", config);
}

ApiClient::Response ApiClient::getAuthStats() {
    return get("/v1/auth/stats");
}

ApiClient::Response ApiClient::clearRateLimit(const std::optional<std::string>& client_ip) {
    nlohmann::json body;
    body["action"] = "clear_rate_limit";
    if (client_ip.has_value()) {
        body["client_ip"] = client_ip.value();
    } else {
        body["clear_all"] = true;
    }
    return post("/v1/auth/clear", body);
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
    } else {
        response.status_code = -1;
        response.body = {{"error", "Request failed"}};
    }
    
    return response;
}

httplib::Headers ApiClient::getHeaders() const {
    httplib::Headers headers;
    headers.emplace("Content-Type", "application/json");
    return headers;
}

} // namespace test
} // namespace kolosal