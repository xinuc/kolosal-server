#ifndef KOLOSAL_CONTROLLERS_DOWNLOADS_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_DOWNLOADS_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"
#include <string>
#include <memory>

namespace kolosal {

// Forward declarations
class DownloadManager;
struct DownloadProgress;

namespace controllers {

class DownloadsController : public BaseController {
public:
    explicit DownloadsController(DownloadManager* download_manager = nullptr);
    ~DownloadsController();

    // Get all downloads status
    Response getAllDownloads();
    
    // Get specific download progress
    Response getDownloadProgress(const std::string& model_id);
    
    // Cancel specific download
    Response cancelDownload(const std::string& model_id);
    
    // Pause specific download
    Response pauseDownload(const std::string& model_id);
    
    // Resume specific download
    Response resumeDownload(const std::string& model_id);
    
    // Cancel all downloads
    Response cancelAllDownloads();

private:
    DownloadManager* download_manager_;
    
    // Helper methods
    nlohmann::json formatDownloadProgress(const std::shared_ptr<DownloadProgress>& progress);
    std::string inferModelType(const std::shared_ptr<DownloadProgress>& progress);
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_DOWNLOADS_CONTROLLER_HPP