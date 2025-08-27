#include "kolosal/controllers/server_logs_controller.hpp"
#include "kolosal/logger.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace kolosal {
namespace controllers {

BaseController::Response ServerLogsController::getLogs() {
    try {
        // Get the ServerLogger instance and retrieve logs
        auto &logger = ServerLogger::instance();
        const auto &logs = logger.getLogs();
        
        nlohmann::json logsList = nlohmann::json::array();
        for (const auto &logEntry : logs) {
            std::string levelStr;
            switch (logEntry.level) {
                case LogLevel::SERVER_ERROR:
                    levelStr = "ERROR";
                    break;
                case LogLevel::SERVER_WARNING:
                    levelStr = "WARNING";
                    break;
                case LogLevel::SERVER_INFO:
                    levelStr = "INFO";
                    break;
                case LogLevel::SERVER_DEBUG:
                    levelStr = "DEBUG";
                    break;
                default:
                    levelStr = "UNKNOWN";
                    break;
            }
            
            nlohmann::json logObject = {
                {"level", levelStr},
                {"timestamp", logEntry.timestamp},
                {"message", logEntry.message}
            };
            
            logsList.push_back(logObject);
        }
        
        // Get current timestamp for the response
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        std::string currentTimestamp = ss.str();
        
        nlohmann::json response = {
            {"logs", logsList},
            {"total_count", logsList.size()},
            {"retrieved_at", currentTimestamp}
        };
        
        ServerLogger::logDebug("Successfully retrieved %zu log entries", logsList.size());
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error retrieving logs: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response ServerLogsController::getLogs(int limit, const std::string& level) {
    try {
        // Get the ServerLogger instance and retrieve logs
        auto &logger = ServerLogger::instance();
        const auto &logs = logger.getLogs();
        
        nlohmann::json logsList = nlohmann::json::array();
        int count = 0;
        
        // Process logs in reverse order (most recent first)
        for (auto it = logs.rbegin(); it != logs.rend() && count < limit; ++it) {
            const auto &logEntry = *it;
            
            std::string levelStr;
            switch (logEntry.level) {
                case LogLevel::SERVER_ERROR:
                    levelStr = "ERROR";
                    break;
                case LogLevel::SERVER_WARNING:
                    levelStr = "WARNING";
                    break;
                case LogLevel::SERVER_INFO:
                    levelStr = "INFO";
                    break;
                case LogLevel::SERVER_DEBUG:
                    levelStr = "DEBUG";
                    break;
                default:
                    levelStr = "UNKNOWN";
                    break;
            }
            
            // Filter by level if specified
            if (!level.empty() && levelStr != level) {
                continue;
            }
            
            nlohmann::json logObject = {
                {"level", levelStr},
                {"timestamp", logEntry.timestamp},
                {"message", logEntry.message}
            };
            
            logsList.push_back(logObject);
            count++;
        }
        
        // Get current timestamp for the response
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        std::string currentTimestamp = ss.str();
        
        nlohmann::json response = {
            {"logs", logsList},
            {"total_count", logsList.size()},
            {"retrieved_at", currentTimestamp}
        };
        
        if (!level.empty()) {
            response["filter"] = {{"level", level}};
        }
        if (limit > 0) {
            response["limit"] = limit;
        }
        
        ServerLogger::logDebug("Successfully retrieved %zu log entries", logsList.size());
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error retrieving logs: %s", e.what());
        return serverError(e.what());
    }
}

nlohmann::json ServerLogsController::formatLogEntry(const std::string& level, 
                                                     const std::string& timestamp, 
                                                     const std::string& message) {
    return {
        {"level", level},
        {"timestamp", timestamp},
        {"message", message}
    };
}

} // namespace controllers
} // namespace kolosal