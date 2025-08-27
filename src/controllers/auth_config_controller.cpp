#include "kolosal/controllers/auth_config_controller.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/auth/auth_middleware.hpp"
#include "kolosal/logger.hpp"
#include <chrono>

namespace kolosal {
namespace controllers {

AuthConfigController::AuthConfigController(auth::AuthMiddleware* auth_middleware)
    : auth_middleware_(auth_middleware) {
    if (!auth_middleware_) {
        try {
            auth_middleware_ = &ServerAPI::instance().getAuthMiddleware();
        } catch (...) {
            // AuthMiddleware might not be available in some contexts
        }
    }
}

BaseController::Response AuthConfigController::getConfig() {
    try {
        if (!auth_middleware_) {
            return serverError("Auth middleware not available");
        }
        
        auto& rateLimiter = auth_middleware_->getRateLimiter();
        auto& corsHandler = auth_middleware_->getCorsHandler();
        
        auto rateLimiterConfig = rateLimiter.getConfig();
        auto corsConfig = corsHandler.getConfig();
        auto apiKeyConfig = auth_middleware_->getApiKeyConfig();
        
        nlohmann::json response = {
            {"rate_limiter", {
                {"enabled", rateLimiterConfig.enabled},
                {"max_requests", rateLimiterConfig.maxRequests},
                {"window_size", rateLimiterConfig.windowSize.count()}
            }},
            {"cors", {
                {"enabled", corsConfig.enabled},
                {"allowed_origins", corsConfig.allowedOrigins},
                {"allowed_methods", corsConfig.allowedMethods},
                {"allowed_headers", corsConfig.allowedHeaders},
                {"allow_credentials", corsConfig.allowCredentials},
                {"max_age", corsConfig.maxAge}
            }},
            {"api_key", {
                {"enabled", apiKeyConfig.enabled},
                {"required", apiKeyConfig.required},
                {"header_name", apiKeyConfig.headerName},
                {"keys_count", apiKeyConfig.validKeys.size()}
            }}
        };
        
        ServerLogger::logInfo("Sent auth config response");
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting auth config: %s", e.what());
        return serverError("Failed to get config: " + std::string(e.what()));
    }
}

BaseController::Response AuthConfigController::updateConfig(const std::string& body) {
    try {
        if (body.empty()) {
            return badRequest("Request body is empty");
        }
        
        auto json = parseJsonBody(body);
        return updateConfig(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error updating auth config: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response AuthConfigController::updateConfig(const nlohmann::json& j) {
    try {
        if (!auth_middleware_) {
            return serverError("Auth middleware not available");
        }
        
        std::string error;
        
        // Validate and update rate limiter config
        if (j.contains("rate_limiter")) {
            if (!validateRateLimiterConfig(j["rate_limiter"], error)) {
                return badRequest(error);
            }
            
            auto& rateLimiter = auth_middleware_->getRateLimiter();
            auto config = rateLimiter.getConfig();
            
            const auto& rl = j["rate_limiter"];
            if (rl.contains("enabled")) {
                config.enabled = rl["enabled"];
            }
            if (rl.contains("max_requests")) {
                config.maxRequests = rl["max_requests"];
            }
            if (rl.contains("window_size")) {
                config.windowSize = std::chrono::seconds(rl["window_size"]);
            }
            
            rateLimiter.updateConfig(config);
        }
        
        // Validate and update CORS config
        if (j.contains("cors")) {
            if (!validateCorsConfig(j["cors"], error)) {
                return badRequest(error);
            }
            
            auto& corsHandler = auth_middleware_->getCorsHandler();
            auto config = corsHandler.getConfig();
            
            const auto& cors = j["cors"];
            if (cors.contains("enabled")) {
                config.enabled = cors["enabled"];
            }
            if (cors.contains("allowed_origins")) {
                config.allowedOrigins.clear();
                for (const auto& origin : cors["allowed_origins"]) {
                    config.allowedOrigins.push_back(origin);
                }
            }
            if (cors.contains("allowed_methods")) {
                config.allowedMethods.clear();
                for (const auto& method : cors["allowed_methods"]) {
                    config.allowedMethods.push_back(method);
                }
            }
            if (cors.contains("allowed_headers")) {
                config.allowedHeaders.clear();
                for (const auto& header : cors["allowed_headers"]) {
                    config.allowedHeaders.push_back(header);
                }
            }
            if (cors.contains("allow_credentials")) {
                config.allowCredentials = cors["allow_credentials"];
            }
            if (cors.contains("max_age")) {
                config.maxAge = cors["max_age"];
            }
            
            corsHandler.updateConfig(config);
        }
        
        // Validate and update API key config
        if (j.contains("api_key")) {
            if (!validateApiKeyConfig(j["api_key"], error)) {
                return badRequest(error);
            }
            
            auto config = auth_middleware_->getApiKeyConfig();
            
            const auto& apiKey = j["api_key"];
            if (apiKey.contains("enabled")) {
                config.enabled = apiKey["enabled"];
            }
            if (apiKey.contains("required")) {
                config.required = apiKey["required"];
            }
            if (apiKey.contains("header_name")) {
                config.headerName = apiKey["header_name"];
            }
            if (apiKey.contains("api_keys")) {
                config.validKeys.clear();
                for (const auto& key : apiKey["api_keys"]) {
                    if (key.is_string() && !key.empty()) {
                        config.validKeys.insert(key);
                    }
                }
            }
            
            auth_middleware_->updateApiKeyConfig(config);
        }
        
        nlohmann::json response = {
            {"message", "Authentication configuration updated successfully"},
            {"status", "success"}
        };
        
        ServerLogger::logInfo("Updated auth configuration");
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error updating auth config: %s", e.what());
        return serverError("Failed to update config: " + std::string(e.what()));
    }
}

BaseController::Response AuthConfigController::getStats() {
    try {
        if (!auth_middleware_) {
            return serverError("Auth middleware not available");
        }
        
        auto& rateLimiter = auth_middleware_->getRateLimiter();
        auto stats = rateLimiter.getStatistics();
        
        nlohmann::json clientsJson = nlohmann::json::object();
        int totalRequests = 0;
        for (const auto& client : stats) {
            clientsJson[client.first] = {
                {"request_count", client.second}
            };
            totalRequests += client.second;
        }
        
        nlohmann::json response = {
            {"rate_limit_stats", {
                {"total_clients", stats.size()},
                {"total_requests", totalRequests},
                {"clients", clientsJson}
            }},
            {"cors_stats", {
                {"message", "CORS statistics not implemented yet"}
            }}
        };
        
        ServerLogger::logInfo("Served auth statistics");
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting auth stats: %s", e.what());
        return serverError("Failed to get stats: " + std::string(e.what()));
    }
}

BaseController::Response AuthConfigController::clearRateLimit(const std::string& body) {
    try {
        if (body.empty()) {
            // Clear all rate limits
            return clearRateLimit(nlohmann::json::object());
        }
        
        auto json = parseJsonBody(body);
        return clearRateLimit(json);
        
    } catch (const std::invalid_argument& e) {
        return badRequest(e.what());
    } catch (const std::exception& e) {
        ServerLogger::logError("Error clearing rate limit: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response AuthConfigController::clearRateLimit(const nlohmann::json& request) {
    try {
        if (!auth_middleware_) {
            return serverError("Auth middleware not available");
        }
        
        auto& rateLimiter = auth_middleware_->getRateLimiter();
        
        if (request.contains("ip") && request["ip"].is_string()) {
            std::string ip = request["ip"];
            rateLimiter.clearClient(ip);
            
            nlohmann::json response = {
                {"message", "Rate limit cleared for IP: " + ip},
                {"status", "success"}
            };
            
            ServerLogger::logInfo("Cleared rate limit for IP: %s", ip.c_str());
            return ok(response);
        } else {
            // Clear all rate limits
            rateLimiter.clearAll();
            
            nlohmann::json response = {
                {"message", "All rate limits cleared"},
                {"status", "success"}
            };
            
            ServerLogger::logInfo("Cleared all rate limits");
            return ok(response);
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error clearing rate limit: %s", e.what());
        return serverError("Failed to clear rate limit: " + std::string(e.what()));
    }
}

bool AuthConfigController::validateRateLimiterConfig(const nlohmann::json& config, std::string& error) {
    if (config.contains("max_requests") && !config["max_requests"].is_number_unsigned()) {
        error = "max_requests must be a positive integer";
        return false;
    }
    if (config.contains("window_size") && !config["window_size"].is_number_unsigned()) {
        error = "window_size must be a positive integer";
        return false;
    }
    return true;
}

bool AuthConfigController::validateCorsConfig(const nlohmann::json& config, std::string& error) {
    if (config.contains("allowed_origins") && !config["allowed_origins"].is_array()) {
        error = "allowed_origins must be an array";
        return false;
    }
    if (config.contains("allowed_methods") && !config["allowed_methods"].is_array()) {
        error = "allowed_methods must be an array";
        return false;
    }
    if (config.contains("allowed_headers") && !config["allowed_headers"].is_array()) {
        error = "allowed_headers must be an array";
        return false;
    }
    return true;
}

bool AuthConfigController::validateApiKeyConfig(const nlohmann::json& config, std::string& error) {
    if (config.contains("api_keys") && !config["api_keys"].is_array()) {
        error = "api_keys must be an array";
        return false;
    }
    return true;
}

} // namespace controllers
} // namespace kolosal