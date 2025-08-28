#include "kolosal/controllers/downloads_controller.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

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
        
        // Count startup vs regular downloads and model types
        int startup_count = 0;
        int regular_count = 0;
        int embedding_downloads = 0;
        int llm_downloads = 0;
        
        for (const auto& [modelId, progress] : downloads) {
            // Add download_type field for list responses
            nlohmann::json downloadInfo = formatDownloadProgress(progress);
            downloadInfo["download_type"] = progress->engine_params ? "startup" : "regular";
            downloadsArray.push_back(downloadInfo);
            
            // Update statistics
            if (progress->engine_params) {
                startup_count++;
            } else {
                regular_count++;
            }
            
            // Count model types
            const std::string type = progress->engine_params ? progress->engine_params->model_type : inferModelType(progress);
            if (type == "embedding") {
                embedding_downloads++;
            } else {
                llm_downloads++;
            }
        }
        
        nlohmann::json response = {
            {"active_downloads", downloadsArray},
            {"summary", {
                {"total_active", static_cast<int>(downloads.size())},
                {"startup_downloads", startup_count},
                {"regular_downloads", regular_count},
                {"embedding_model_downloads", embedding_downloads},
                {"llm_model_downloads", llm_downloads}
            }},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
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
            ServerLogger::logInfo("Successfully provided download progress for model: %s (%.1f%%)",
                                model_id.c_str(), progress->percentage);
            return ok(formatDownloadProgress(progress));
        } else {
            // Match old error format exactly with error code
            nlohmann::json error = {
                {"error", {
                    {"message", "No download found for model ID: " + model_id},
                    {"type", "not_found_error"},
                    {"param", "model_id"},
                    {"code", "download_not_found"}
                }}
            };
            ServerLogger::logInfo("No download found for model ID: %s", model_id.c_str());
            return Response(404, error);
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
    
    // Calculate elapsed time
    auto now = std::chrono::system_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        (progress->status == "downloading" || progress->status == "creating_engine" ? now : progress->end_time) - progress->start_time)
        .count();
    
    // Calculate download speed (bytes per second)
    double download_speed = elapsed_seconds > 0 ? static_cast<double>(progress->downloaded_bytes) / elapsed_seconds : 0.0;
    
    // Estimate remaining time (only for active downloads)
    int estimated_remaining_seconds = -1;
    if (progress->status == "downloading" && progress->percentage > 0 && download_speed > 0) {
        size_t remaining_bytes = progress->total_bytes - progress->downloaded_bytes;
        estimated_remaining_seconds = static_cast<int>(remaining_bytes / download_speed);
    }
    
    // Ensure percentage is valid before sending response
    double safe_percentage = progress->percentage;
    if (std::isnan(safe_percentage) || std::isinf(safe_percentage) || safe_percentage < 0.0 || safe_percentage > 100.0) {
        ServerLogger::logWarning("Invalid percentage value %.2f for model %s in API response, using 0.0", 
                               progress->percentage, progress->model_id.c_str());
        safe_percentage = 0.0;
    }
    
    // Build response JSON matching old format exactly
    nlohmann::json response = {
        {"model_id", progress->model_id},
        {"type", progress->engine_params ? progress->engine_params->model_type : inferModelType(progress)},
        {"status", progress->status},
        {"url", progress->url},
        {"local_path", progress->local_path},
        {"progress", {
            {"downloaded_bytes", progress->downloaded_bytes},
            {"total_bytes", progress->total_bytes},
            {"percentage", safe_percentage},
            {"download_speed_bps", download_speed}
        }},
        {"timing", {
            {"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(progress->start_time.time_since_epoch()).count()},
            {"elapsed_seconds", elapsed_seconds}
        }}
    };
    
    // Add end time and error message if applicable
    if (progress->status != "downloading" && progress->status != "creating_engine") {
        response["timing"]["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(progress->end_time.time_since_epoch()).count();
    }
    
    if (!progress->error_message.empty()) {
        response["error_message"] = progress->error_message;
    }
    
    if (estimated_remaining_seconds >= 0) {
        response["timing"]["estimated_remaining_seconds"] = estimated_remaining_seconds;
    }
    
    // Add engine creation info if applicable
    if (progress->engine_params) {
        nlohmann::json engineInfo = {
            {"model_id", progress->engine_params->model_id},
            {"model_type", progress->engine_params->model_type},
            {"load_immediately", progress->engine_params->load_immediately},
            {"main_gpu_id", progress->engine_params->main_gpu_id},
            {"inference_engine", progress->engine_params->inference_engine}
        };
        
        // Add embedding-specific information
        if (progress->engine_params->model_type == "embedding") {
            engineInfo["embedding_features"] = {
                {"normalization", true},
                {"supports_batching", true},
                {"optimized_for_retrieval", true}
            };
            
            engineInfo["recommended_usage"] = {
                {"document_embedding", true},
                {"semantic_search", true},
                {"similarity_computation", true}
            };
        } else {
            engineInfo["llm_features"] = {
                {"text_generation", true},
                {"chat_completion", true},
                {"instruction_following", true}
            };
        }
        
        response["engine_creation"] = engineInfo;
    }
    
    return response;
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

// These helper methods are no longer needed since we return numeric values
// matching the old API format instead of formatted strings

} // namespace controllers
} // namespace kolosal