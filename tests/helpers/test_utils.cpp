#include "test_utils.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <cstdlib>

namespace kolosal {
namespace test {

std::vector<std::string> TestUtils::temp_files_;

std::string TestUtils::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot read file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void TestUtils::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    file << content;
}

std::string TestUtils::getTempFilePath(const std::string& prefix, const std::string& extension) {
    static int counter = 0;
    std::string temp_dir = std::filesystem::temp_directory_path();
    std::string filename = prefix + "_" + std::to_string(counter++) + "_" + 
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + 
                          extension;
    std::string path = temp_dir + "/" + filename;
    temp_files_.push_back(path);
    return path;
}

void TestUtils::cleanupTempFiles() {
    for (const auto& file : temp_files_) {
        try {
            std::filesystem::remove(file);
        } catch (...) {
            // Ignore errors during cleanup
        }
    }
    temp_files_.clear();
}

nlohmann::json TestUtils::loadJson(const std::string& path) {
    std::string content = readFile(path);
    return nlohmann::json::parse(content);
}

void TestUtils::saveJson(const std::string& path, const nlohmann::json& j) {
    writeFile(path, j.dump(2));
}

nlohmann::json TestUtils::loadFixture(const std::string& name) {
    std::string path = "./fixtures/" + name;
    if (!std::filesystem::exists(path)) {
        path = "../fixtures/" + name;
    }
    if (!std::filesystem::exists(path)) {
        path = "../../tests/fixtures/" + name;
    }
    return loadJson(path);
}

std::string TestUtils::generateRandomString(size_t length) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    return result;
}

std::vector<std::string> TestUtils::generateTextChunks(size_t count, size_t chunk_size) {
    std::vector<std::string> chunks;
    chunks.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        chunks.push_back(generateRandomString(chunk_size));
    }
    return chunks;
}

nlohmann::json TestUtils::generateTestDocument(const std::string& id) {
    std::string doc_id = id.empty() ? "doc_" + generateRandomString(8) : id;
    return {
        {"id", doc_id},
        {"text", "This is a test document with some content. " + generateRandomString(100)},
        {"metadata", {
            {"source", "test"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"category", "test_category"}
        }}
    };
}

nlohmann::json TestUtils::generateTestEmbedding(size_t dimensions) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> embedding;
    embedding.reserve(dimensions);
    for (size_t i = 0; i < dimensions; ++i) {
        embedding.push_back(dist(gen));
    }
    
    return {
        {"embedding", embedding},
        {"model", "text-embedding-3-small"},
        {"dimensions", dimensions}
    };
}

nlohmann::json TestUtils::getTestConfig() {
    return {
        {"server", {
            {"host", "127.0.0.1"},
            {"port", 3001},
            {"workers", 2}
        }},
        {"security", {
            {"enabled", false}
        }},
        {"performance", {
            {"request_queue_size", 100}
        }},
        {"default_model_settings", {
            {"temperature", 0.7},
            {"max_tokens", 2048}
        }}
    };
}

nlohmann::json TestUtils::getMinimalConfig() {
    return {
        {"server", {
            {"port", 3001}
        }}
    };
}

nlohmann::json TestUtils::getAuthEnabledConfig() {
    auto config = getTestConfig();
    config["security"]["enabled"] = true;
    config["security"]["api_key"] = {
        {"enabled", true},
        {"required", true},
        {"api_keys", {"test-key-123", "test-key-456"}}
    };
    config["security"]["rate_limiter"] = {
        {"enabled", true},
        {"max_requests", 10},
        {"window_size", 60}
    };
    return config;
}

bool TestUtils::compareJsonIgnoringFields(
    const nlohmann::json& a, 
    const nlohmann::json& b,
    const std::vector<std::string>& ignore_fields) {
    
    if (a.type() != b.type()) {
        return false;
    }
    
    if (!a.is_object()) {
        return a == b;
    }
    
    for (auto& [key, value] : a.items()) {
        // Skip ignored fields
        if (std::find(ignore_fields.begin(), ignore_fields.end(), key) != ignore_fields.end()) {
            continue;
        }
        
        if (!b.contains(key)) {
            return false;
        }
        
        if (value.is_object() && b[key].is_object()) {
            if (!compareJsonIgnoringFields(value, b[key], ignore_fields)) {
                return false;
            }
        } else if (value != b[key]) {
            return false;
        }
    }
    
    // Check that b doesn't have extra fields not in ignore list
    for (auto& [key, value] : b.items()) {
        if (std::find(ignore_fields.begin(), ignore_fields.end(), key) == ignore_fields.end() && 
            !a.contains(key)) {
            return false;
        }
    }
    
    return true;
}

bool TestUtils::compareFloatArrays(
    const std::vector<float>& a,
    const std::vector<float>& b,
    float tolerance) {
    
    if (a.size() != b.size()) {
        return false;
    }
    
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::abs(a[i] - b[i]) > tolerance) {
            return false;
        }
    }
    
    return true;
}

// TestFixture implementation
TestFixture::TestFixture() {
    fixture_dir_ = "./fixtures";
    temp_dir_ = std::filesystem::temp_directory_path() / "kolosal_tests";
    setUp();
}

TestFixture::~TestFixture() {
    tearDown();
}

void TestFixture::setUp() {
    // Create temp directory if it doesn't exist
    std::filesystem::create_directories(temp_dir_);
}

void TestFixture::tearDown() {
    // Clean up temp files
    TestUtils::cleanupTempFiles();
    
    // Remove temp directory if empty
    try {
        std::filesystem::remove(temp_dir_);
    } catch (...) {
        // Directory might not be empty
    }
}

} // namespace test
} // namespace kolosal