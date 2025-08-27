#pragma once

#include "route_interface.hpp"
#include "../logger.hpp"
#include "../utils.hpp"
#include <json.hpp>
#include <map>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

/**
 * Base class for all routes implementing common functionality
 * Follows SOLID principles:
 * - SRP: Only handles HTTP routing concerns
 * - OCP: Extensible through virtual methods
 * - DIP: Depends on abstractions (controllers)
 */
class BaseRoute : public IRoute {
protected:
    std::string current_method_;
    std::string current_path_;
    
    /**
     * Add CORS headers to response
     */
    std::map<std::string, std::string> getCorsHeaders() const {
        return {
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", getAllowedMethods()},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
            {"Access-Control-Max-Age", "86400"}
        };
    }
    
    /**
     * Get allowed methods for this route (for CORS)
     */
    virtual std::string getAllowedMethods() const {
        return "GET, POST, OPTIONS";
    }
    
    /**
     * Log request with thread info
     */
    void logRequest(const std::string& message) const {
        ServerLogger::logDebug("[Thread %u] %s", std::this_thread::get_id(), message.c_str());
    }
    
    /**
     * Log error with thread info
     */
    void logError(const std::string& message) const {
        ServerLogger::logError("[Thread %u] %s", std::this_thread::get_id(), message.c_str());
    }
    
    /**
     * Merge headers helper
     */
    std::map<std::string, std::string> mergeHeaders(
        const std::map<std::string, std::string>& base,
        const std::map<std::string, std::string>& additional) const {
        
        auto result = base;
        for (const auto& [key, value] : additional) {
            result[key] = value;
        }
        return result;
    }
    
    /**
     * Send error response with CORS headers
     */
    void sendErrorResponse(SocketType sock, int status_code, const std::string& error_message, 
                          const std::string& error_type = "server_error") {
        json errorResponse = {
            {"error", {
                {"message", error_message},
                {"type", error_type}
            }}
        };
        
        auto headers = mergeHeaders(getCorsHeaders(), {{"Content-Type", "application/json"}});
        send_response(sock, status_code, errorResponse.dump(), headers);
    }
    
    /**
     * Send success response with CORS headers
     */
    void sendSuccessResponse(SocketType sock, int status_code, const json& response_body) {
        auto headers = mergeHeaders(getCorsHeaders(), {{"Content-Type", "application/json"}});
        send_response(sock, status_code, response_body.dump(), headers);
    }
    
public:
    virtual ~BaseRoute() = default;
};

} // namespace kolosal