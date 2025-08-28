#include "response_validator.hpp"
#include <algorithm>
#include <sstream>

namespace kolosal {
namespace test {

void ResponseValidator::checkRequiredFields(const nlohmann::json& j, const std::vector<std::string>& fields) {
    for (const auto& field : fields) {
        REQUIRE(j.contains(field));
    }
}

// Auth validation
void ResponseValidator::checkAuthConfig(const nlohmann::json& j) {
    checkRequiredFields(j, {"rate_limiter", "cors", "api_key"});
    
    // Validate rate_limiter
    const auto& rl = j["rate_limiter"];
    checkRequiredFields(rl, {"enabled", "max_requests", "window_size"});
    
    // Validate cors
    const auto& cors = j["cors"];
    checkRequiredFields(cors, {"enabled", "allowed_origins"});
    
    // Validate api_key
    const auto& api_key = j["api_key"];
    checkRequiredFields(api_key, {"enabled"});
}

void ResponseValidator::checkAuthStats(const nlohmann::json& j) {
    checkRequiredFields(j, {"rate_limit_stats", "cors_stats"});
    
    const auto& rl = j["rate_limit_stats"];
    checkRequiredFields(rl, {"total_clients", "total_requests", "clients"});
}

} // namespace test
} // namespace kolosal