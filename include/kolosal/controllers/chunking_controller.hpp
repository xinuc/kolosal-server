#ifndef KOLOSAL_CONTROLLERS_CHUNKING_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_CHUNKING_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"
#include "kolosal/retrieval/chunking_types.hpp"
#include "kolosal/node_manager.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>

namespace kolosal {
namespace controllers {

class ChunkingController : public BaseController {
public:
    explicit ChunkingController(NodeManager* node_manager = nullptr);
    ~ChunkingController();

    Response processChunking(const std::string& body);
    Response processChunking(const nlohmann::json& request);

private:
    std::future<std::vector<std::string>> processRegularChunking(
        const std::string& text,
        const std::string& model_name,
        int chunk_size,
        int overlap
    );

    std::future<std::vector<std::string>> processSemanticChunking(
        const std::string& text,
        const std::string& model_name,
        int chunk_size,
        int overlap,
        int max_chunk_size,
        float similarity_threshold
    );

    bool validateChunkingModel(const std::string& model_name) const;
    int estimateTokenCount(const std::string& text) const;
    std::string generateRequestId();
    
    NodeManager* node_manager_;
    std::unique_ptr<retrieval::ChunkingService> chunking_service_;
    mutable std::mutex service_mutex_;
    std::atomic<int> request_counter_;
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_CHUNKING_CONTROLLER_HPP