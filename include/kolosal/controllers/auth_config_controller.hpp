#ifndef KOLOSAL_CONTROLLERS_AUTH_CONFIG_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_AUTH_CONFIG_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"

namespace kolosal {

namespace auth {
    class AuthMiddleware;
}

namespace controllers {

class AuthConfigController : public BaseController {
public:
    explicit AuthConfigController(auth::AuthMiddleware* auth_middleware = nullptr);
    ~AuthConfigController() = default;

    // Get current auth configuration
    Response getConfig();
    
    // Update auth configuration
    Response updateConfig(const std::string& body);
    Response updateConfig(const nlohmann::json& config);
    
    // Get auth statistics
    Response getStats();
    
    // Clear rate limit for specific IP or all
    Response clearRateLimit(const std::string& body);
    Response clearRateLimit(const nlohmann::json& request);

private:
    auth::AuthMiddleware* auth_middleware_;
    
    // Helper methods
    bool validateRateLimiterConfig(const nlohmann::json& config, std::string& error);
    bool validateCorsConfig(const nlohmann::json& config, std::string& error);
    bool validateApiKeyConfig(const nlohmann::json& config, std::string& error);
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_AUTH_CONFIG_CONTROLLER_HPP