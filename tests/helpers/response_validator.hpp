#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace kolosal {
namespace test {

class ResponseValidator {
public:
    // Auth validation
    static void checkAuthConfig(const nlohmann::json& j);
    static void checkAuthStats(const nlohmann::json& j);

    
    // Internal helpers (used by auth validation methods)
    static void checkRequiredFields(const nlohmann::json& j, const std::vector<std::string>& fields);
};

} // namespace test
} // namespace kolosal