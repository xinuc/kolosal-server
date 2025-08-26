#pragma once

#include "llm_metrics.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <atomic>

namespace kolosal {
namespace metrics {

/**
 * RAII wrapper for tracking LLM requests
 * Automatically tracks request lifecycle, timing, and cleanup
 */
class LLMRequestTracker {
public:
    // Constructor starts tracking
    LLMRequestTracker(const std::string& request_type = "llm", size_t max_tokens = 0) 
        : request_id_(generate_id(request_type)),
          collector_(LLMMetrics::instance().collector()),
          finished_(false) {
        collector_.record_request_start(request_id_, 0, max_tokens);
        collector_.update_active_requests(1, 0);
    }
    
    // Destructor ensures cleanup
    ~LLMRequestTracker() {
        if (!finished_) {
            finish(FinishReason::ERROR);
        }
    }
    
    // Simple one-liners for events
    void first_token() { collector_.record_first_token(request_id_); }
    void token_generated() { collector_.record_token_generated(request_id_); }
    void scheduled() { collector_.record_request_scheduled(request_id_); }
    
    // Mark as finished
    void finish(FinishReason reason = FinishReason::COMPLETED) {
        if (!finished_) {
            collector_.record_request_finished(request_id_, reason);
            collector_.update_active_requests(0, 0);
            finished_ = true;
        }
    }
    
    const std::string& id() const { return request_id_; }
    
private:
    static std::string generate_id(const std::string& type) {
        static std::atomic<uint64_t> counter{0};
        return type + "_" + std::to_string(++counter) + "_" + 
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    std::string request_id_;
    LLMMetricsCollector& collector_;
    bool finished_;
    
    // Disable copy
    LLMRequestTracker(const LLMRequestTracker&) = delete;
    LLMRequestTracker& operator=(const LLMRequestTracker&) = delete;
};

/**
 * Scoped timer for measuring durations
 * Automatically records elapsed time when destroyed
 */
class ScopedTimer {
public:
    using Callback = std::function<void(double)>;
    
    explicit ScopedTimer(Callback on_complete)
        : start_(std::chrono::steady_clock::now()),
          callback_(on_complete) {}
    
    ~ScopedTimer() {
        if (callback_) {
            auto duration = std::chrono::steady_clock::now() - start_;
            double seconds = std::chrono::duration<double>(duration).count();
            callback_(seconds);
        }
    }
    
    void cancel() { callback_ = nullptr; }
    
private:
    std::chrono::steady_clock::time_point start_;
    Callback callback_;
};

/**
 * Simple batch metrics helper
 */
class BatchMetrics {
public:
    static void record_batch(size_t batch_size) {
        LLMMetrics::instance().collector().record_batch_size(batch_size);
    }
    
    static void record_iteration(size_t tokens, double duration_ms) {
        LLMMetrics::instance().collector().record_iteration(tokens, duration_ms);
    }
};

} // namespace metrics
} // namespace kolosal