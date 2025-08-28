#include "test_server.hpp"
#include "api_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <csignal>
#include <filesystem>

namespace kolosal {
namespace test {

TestServer::TestServer(int port, const std::string& config_path)
    : port_(port)
    , config_path_(config_path)
    , running_(false)
    , server_pid_(-1) {
    findServerBinary();
}

TestServer::~TestServer() {
    if (running_) {
        stop();
    }
}

bool TestServer::start() {
    if (running_) {
        return true;
    }
    
    if (!findServerBinary()) {
        std::cerr << "Server binary not found" << std::endl;
        return false;
    }
    
    // Build command
    std::string cmd = server_binary_;
    cmd += " --port " + std::to_string(port_);
    if (!config_path_.empty()) {
        cmd += " --config " + config_path_;
    }
    cmd += " > /tmp/kolosal_test_server.log 2>&1 & echo $!";
    
    // Start server in background
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to start server" << std::endl;
        return false;
    }
    
    char buffer[128];
    if (fgets(buffer, 128, pipe) != nullptr) {
        server_pid_ = std::atoi(buffer);
    }
    pclose(pipe);
    
    if (server_pid_ <= 0) {
        std::cerr << "Failed to get server PID" << std::endl;
        return false;
    }
    
    running_ = true;
    return true;
}

void TestServer::stop() {
    if (!running_ || server_pid_ <= 0) {
        return;
    }
    
    // Send SIGTERM to server process
    kill(server_pid_, SIGTERM);
    
    // Wait a bit for graceful shutdown
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Force kill if still running
    kill(server_pid_, SIGKILL);
    
    running_ = false;
    server_pid_ = -1;
}

bool TestServer::isRunning() const {
    return running_;
}

bool TestServer::waitForReady(int timeout_seconds) {
    if (!running_) {
        return false;
    }
    
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (checkServerReady()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return false;
}

std::string TestServer::getBaseUrl() const {
    return "http://localhost:" + std::to_string(port_);
}

void TestServer::setConfigPath(const std::string& path) {
    config_path_ = path;
}

void TestServer::setEnvironment(const std::string& key, const std::string& value) {
    setenv(key.c_str(), value.c_str(), 1);
}

bool TestServer::findServerBinary() {
    // Look for server binary in common locations
    std::vector<std::string> candidates = {
        "./build/kolosal-server",
        "./build/Release/kolosal-server",
        "./build/Debug/kolosal-server",
        "../build/kolosal-server",
        "../build/Release/kolosal-server",
        "../build/Debug/kolosal-server",
        "../../build/kolosal-server",
        "../../build/Release/kolosal-server",
        "../../build/Debug/kolosal-server"
    };
    
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
            server_binary_ = std::filesystem::absolute(path);
            return true;
        }
    }
    
    return false;
}

bool TestServer::checkServerReady() {
    try {
        ApiClient client("localhost", port_);
        client.setTimeout(1);
        auto response = client.getServerStats();
        return response.status_code == 200;
    } catch (...) {
        return false;
    }
}

} // namespace test
} // namespace kolosal