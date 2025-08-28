#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

namespace kolosal {
namespace test {

class TestUtils {
public:
    // File utilities
    static std::string readFile(const std::string& path);
    static void writeFile(const std::string& path, const std::string& content);
    static std::string getTempFilePath(const std::string& prefix = "test", const std::string& extension = ".txt");
    static void cleanupTempFiles();
    
    // JSON utilities
    static nlohmann::json loadJson(const std::string& path);
    static void saveJson(const std::string& path, const nlohmann::json& j);
    static nlohmann::json loadFixture(const std::string& name);
    
    // Test data generators
    static std::string generateRandomString(size_t length);
    static std::vector<std::string> generateTextChunks(size_t count, size_t chunk_size);
    static nlohmann::json generateTestDocument(const std::string& id = "");
    static nlohmann::json generateTestEmbedding(size_t dimensions = 1536);
    
    // Configuration helpers
    static nlohmann::json getTestConfig();
    static nlohmann::json getMinimalConfig();
    static nlohmann::json getAuthEnabledConfig();
    
    // Timing utilities
    template<typename F>
    static double measureTime(F&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }
    
    // Comparison utilities
    static bool compareJsonIgnoringFields(
        const nlohmann::json& a, 
        const nlohmann::json& b,
        const std::vector<std::string>& ignore_fields = {}
    );
    
    static bool compareFloatArrays(
        const std::vector<float>& a,
        const std::vector<float>& b,
        float tolerance = 1e-6f
    );
    
private:
    static std::vector<std::string> temp_files_;
};

// Test fixture base class
class TestFixture {
public:
    TestFixture();
    virtual ~TestFixture();
    
protected:
    virtual void setUp();
    virtual void tearDown();
    
    std::string fixture_dir_;
    std::string temp_dir_;
};

} // namespace test
} // namespace kolosal