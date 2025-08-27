#include "kolosal/controllers/downloads_controller.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace kolosal {
namespace controllers {

DownloadsController::DownloadsController(DownloadManager* download_manager)
    : download_manager_(download_manager) {
    if (!download_manager_) {
        download_manager_ = &DownloadManager::getInstance();
    }
}

DownloadsController::~DownloadsController() = default;

BaseController::Response DownloadsController::getAllDownloads() {
    try {
        auto downloads = download_manager_->getAllActiveDownloads();
        nlohmann::json downloadsArray = nlohmann::json::array();
        
        for (const auto& [modelId, progress] : downloads) {
            downloadsArray.push_back(formatDownloadProgress(progress));
        }
        
        nlohmann::json response = {
            {"downloads", downloadsArray},
            {"total", downloadsArray.size()}
        };
        
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting all downloads: %s", e.what());
        return serverError(e.what());
    }
}

BaseController::Response DownloadsController::getDownloadProgress(const std::string& model_id) {
    try {
        auto progress = download_manager_->getDownloadProgress(model_id);
        
        if (progress) {
            return ok(formatDownloadProgress(progress));
        } else {
            return notFound("Download not found for model: " + model_id, "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting download progress for %s: %s", 
                              model_id.c_str(), e.what());
        return serverError(e.what());
    }
}

BaseController::Response DownloadsController::cancelDownload(const std::string& model_id) {
    try {
        bool success = download_manager_->cancelDownload(model_id);
        
        if (success) {
            nlohmann::json response = {
                {"message", "Download cancelled successfully"},
                {"model_id", model_id},
                {"status", "cancelled"}
            };
            
            ServerLogger::logInfo("Cancelled download for model: %s", model_id.c_str());
            return ok(response);
        } else {
            return notFound("No active download found for model: " + model_id, "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error cancelling download for %s: %s", 
                              model_id.c_str(), e.what());
        return serverError(e.what());
    }
}

BaseController::Response DownloadsController::pauseDownload(const std::string& model_id) {
    try {
        bool success = download_manager_->pauseDownload(model_id);
        
        if (success) {
            nlohmann::json response = {
                {"message", "Download paused successfully"},
                {"model_id", model_id},
                {"status", "paused"}
            };
            
            ServerLogger::logInfo("Paused download for model: %s", model_id.c_str());
            return ok(response);
        } else {
            return notFound("No active download found for model: " + model_id, "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error pausing download for %s: %s", 
                              model_id.c_str(), e.what());
        return serverError(e.what());
    }
}

BaseController::Response DownloadsController::resumeDownload(const std::string& model_id) {
    try {
        bool success = download_manager_->resumeDownload(model_id);
        
        if (success) {
            nlohmann::json response = {
                {"message", "Download resumed successfully"},
                {"model_id", model_id},
                {"status", "downloading"}
            };
            
            ServerLogger::logInfo("Resumed download for model: %s", model_id.c_str());
            return ok(response);
        } else {
            return notFound("No paused download found for model: " + model_id, "model_id");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error resuming download for %s: %s", 
                              model_id.c_str(), e.what());
        return serverError(e.what());
    }
}

BaseController::Response DownloadsController::cancelAllDownloads() {
    try {
        int cancelledCount = download_manager_->cancelAllDownloads();
        
        nlohmann::json response = {
            {"message", "All downloads cancelled"},
            {"cancelled_count", cancelledCount}
        };
        
        ServerLogger::logInfo("Cancelled all %d downloads", cancelledCount);
        return ok(response);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error cancelling all downloads: %s", e.what());
        return serverError(e.what());
    }
}

nlohmann::json DownloadsController::formatDownloadProgress(const std::shared_ptr<DownloadProgress>& progress) {
    if (!progress) {
        return nlohmann::json::object();
    }
    
    double percentage = 0.0;
    if (progress->total_bytes > 0) {
        percentage = (static_cast<double>(progress->downloaded_bytes) / progress->total_bytes) * 100.0;
    }
    
    nlohmann::json result = {
        {"model_id", progress->model_id},
        {"url", progress->url},
        {"local_path", progress->local_path},
        {"status", progress->status},
        {"downloaded_size", progress->downloaded_bytes},
        {"total_size", progress->total_bytes},
        {"percentage", percentage},
        {"error", progress->error_message}
    };
    
    // Add speed and time remaining if downloading
    if (progress->status == "downloading" && progress->total_bytes > 0) {
        // Calculate download speed if possible
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress->start_time).count();
        if (elapsed > 0) {
            double download_speed = static_cast<double>(progress->downloaded_bytes) / elapsed;
            result["download_speed"] = formatDownloadSpeed(download_speed);
            
            if (progress->total_bytes > progress->downloaded_bytes) {
                double remainingBytes = progress->total_bytes - progress->downloaded_bytes;
                double remainingTime = remainingBytes / download_speed;
                result["time_remaining"] = formatRemainingTime(remainingTime);
            }
        }
    }
    
    // Add model type
    result["model_type"] = inferModelType(progress);
    
    // Add engine params if available
    if (progress->engine_params) {
        result["engine_params"] = {
            {"model_type", progress->engine_params->model_type},
            {"load_immediately", progress->engine_params->load_immediately},
            {"main_gpu_id", progress->engine_params->main_gpu_id},
            {"inference_engine", progress->engine_params->inference_engine}
        };
    }
    
    return result;
}

std::string DownloadsController::inferModelType(const std::shared_ptr<DownloadProgress>& progress) {
    if (progress && progress->engine_params) {
        return progress->engine_params->model_type;
    }
    
    auto looksEmbedding = [](const std::string& s) -> bool {
        if (s.empty()) return false;
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.find("embedding") != std::string::npos ||
               lower.find("embed") != std::string::npos ||
               lower.find("text-embedding") != std::string::npos ||
               lower.find("retrieval") != std::string::npos ||
               lower.find("nomic-embed") != std::string::npos ||
               lower.find("e5") != std::string::npos ||
               lower.find("gte-") != std::string::npos;
    };
    
    if (progress) {
        if (looksEmbedding(progress->model_id) ||
            looksEmbedding(progress->url) ||
            looksEmbedding(progress->local_path)) {
            return "embedding";
        }
    }
    return "llm"; // default
}

std::string DownloadsController::formatDownloadSpeed(double bytesPerSec) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    
    if (bytesPerSec >= GB) {
        ss << (bytesPerSec / GB) << " GB/s";
    } else if (bytesPerSec >= MB) {
        ss << (bytesPerSec / MB) << " MB/s";
    } else if (bytesPerSec >= KB) {
        ss << (bytesPerSec / KB) << " KB/s";
    } else {
        ss << bytesPerSec << " B/s";
    }
    
    return ss.str();
}

std::string DownloadsController::formatRemainingTime(double seconds) {
    if (std::isinf(seconds) || std::isnan(seconds)) {
        return "unknown";
    }
    
    int hours = static_cast<int>(seconds) / 3600;
    int minutes = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;
    
    std::ostringstream ss;
    if (hours > 0) {
        ss << hours << "h " << minutes << "m " << secs << "s";
    } else if (minutes > 0) {
        ss << minutes << "m " << secs << "s";
    } else {
        ss << secs << "s";
    }
    
    return ss.str();
}

} // namespace controllers
} // namespace kolosal