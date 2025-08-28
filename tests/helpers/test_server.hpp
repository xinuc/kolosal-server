#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>

namespace kolosal {
namespace test {

class TestServer {
public:
    TestServer(int port = 3001, const std::string& config_path = "");
    ~TestServer();
    
    // Start and stop the test server
    bool start();
    void stop();
    bool isRunning() const;
    
    // Wait for server to be ready
    bool waitForReady(int timeout_seconds = 30);
    
    // Get server info
    int getPort() const { return port_; }
    std::string getBaseUrl() const;
    
    // Configuration
    void setConfigPath(const std::string& path);
    void setEnvironment(const std::string& key, const std::string& value);
    
private:
    int port_;
    std::string config_path_;
    std::string server_binary_;
    std::unique_ptr<std::thread> server_thread_;
    std::atomic<bool> running_;
    int server_pid_;
    
    bool findServerBinary();
    bool checkServerReady();
};

// RAII helper for starting/stopping server
class ScopedTestServer {
public:
    ScopedTestServer(int port = 3001, const std::string& config = "")
        : server_(port, config) {
        server_.start();
        server_.waitForReady();
    }
    
    ~ScopedTestServer() {
        server_.stop();
    }
    
    TestServer& get() { return server_; }
    int getPort() const { return server_.getPort(); }
    std::string getBaseUrl() const { return server_.getBaseUrl(); }
    
private:
    TestServer server_;
};

} // namespace test
} // namespace kolosal