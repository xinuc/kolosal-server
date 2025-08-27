#pragma once

#include <string>
#include <map>
#include <json.hpp>

namespace kolosal {
namespace controllers {

/**
 * Base controller class providing common functionality for all controllers
 * This abstracts the business logic from the HTTP handling layer
 */
class BaseController {
public:
    /**
     * Response structure that can be used by any HTTP server implementation
     */
    struct Response {
        int status_code;
        nlohmann::json body;
        std::map<std::string, std::string> headers;
        
        Response(int code = 200) : status_code(code) {
            headers["Content-Type"] = "application/json";
        }
        
        Response(int code, const nlohmann::json& json_body) 
            : status_code(code), body(json_body) {
            headers["Content-Type"] = "application/json";
        }
    };
    
    virtual ~BaseController() = default;
    
protected:
    /**
     * Helper methods for creating standard responses
     */
    Response ok(const nlohmann::json& body) {
        return Response(200, body);
    }
    
    Response created(const nlohmann::json& body) {
        return Response(201, body);
    }
    
    Response accepted(const nlohmann::json& body) {
        return Response(202, body);
    }
    
    Response noContent() {
        return Response(204);
    }
    
    Response badRequest(const std::string& message, const std::string& param = "") {
        nlohmann::json error = {
            {"error", {
                {"message", message},
                {"type", "invalid_request_error"}
            }}
        };
        if (!param.empty()) {
            error["error"]["param"] = param;
        }
        return Response(400, error);
    }
    
    Response unauthorized(const std::string& message) {
        return Response(401, {
            {"error", {
                {"message", message},
                {"type", "authentication_error"}
            }}
        });
    }
    
    Response forbidden(const std::string& message) {
        return Response(403, {
            {"error", {
                {"message", message},
                {"type", "permission_error"}
            }}
        });
    }
    
    Response notFound(const std::string& message, const std::string& param = "") {
        nlohmann::json error = {
            {"error", {
                {"message", message},
                {"type", "not_found_error"}
            }}
        };
        if (!param.empty()) {
            error["error"]["param"] = param;
        }
        return Response(404, error);
    }
    
    Response conflict(const std::string& message) {
        return Response(409, {
            {"error", {
                {"message", message},
                {"type", "conflict_error"}
            }}
        });
    }
    
    Response tooManyRequests(const std::string& message) {
        return Response(429, {
            {"error", {
                {"message", message},
                {"type", "rate_limit_error"}
            }}
        });
    }
    
    Response serverError(const std::string& message) {
        return Response(500, {
            {"error", {
                {"message", message},
                {"type", "server_error"}
            }}
        });
    }
    
    Response serviceUnavailable(const std::string& message) {
        return Response(503, {
            {"error", {
                {"message", message},
                {"type", "service_unavailable"}
            }}
        });
    }
    
    /**
     * Parse JSON from string with error handling
     */
    nlohmann::json parseJsonBody(const std::string& body) {
        if (body.empty()) {
            throw std::invalid_argument("Request body is empty");
        }
        
        try {
            return nlohmann::json::parse(body);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::invalid_argument("Invalid JSON: " + std::string(e.what()));
        }
    }
};

} // namespace controllers
} // namespace kolosal