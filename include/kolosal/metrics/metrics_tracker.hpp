#pragma once

#include "http_metrics.hpp"
#include "llm_metrics.hpp"
#include "system_metrics.hpp"
#include "request_tracker.hpp"
#include "../utils.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <atomic>

namespace kolosal {
namespace metrics {

// HTTPRequestTracker is already defined in http_metrics.hpp with complete_from_context() support

/**
 * RAII wrapper for system metrics collection
 * Note: SystemMetricsCollector is managed elsewhere, this just provides a convenient interface
 */
class SystemMetricsSnapshot {
public:
    SystemMetricsSnapshot(bool collect_now = false) {
        // System metrics are collected in the system_metrics_route
        // This is just a placeholder for consistent API
    }
    
    ~SystemMetricsSnapshot() {
        // Could optionally trigger collection on exit
    }
    
    static void collect() {
        // System metrics collection is handled by system_metrics_route
        // This method provides a consistent interface but doesn't do anything
    }
};

/**
 * Simple scoped timer for custom metrics
 */
template<typename Callback>
class ScopedMetricTimer {
public:
    explicit ScopedMetricTimer(Callback callback)
        : start_(std::chrono::steady_clock::now()),
          callback_(std::move(callback)),
          cancelled_(false) {}
    
    ~ScopedMetricTimer() {
        if (!cancelled_) {
            auto duration = std::chrono::steady_clock::now() - start_;
            callback_(std::chrono::duration<double>(duration).count());
        }
    }
    
    void cancel() { cancelled_ = true; }
    
private:
    std::chrono::steady_clock::time_point start_;
    Callback callback_;
    bool cancelled_;
};

/**
 * Helper to create scoped timers with lambdas
 */
template<typename Callback>
auto make_timer(Callback&& callback) {
    return ScopedMetricTimer<Callback>(std::forward<Callback>(callback));
}

/**
 * Combined tracker for both HTTP and LLM metrics
 * Use when handling LLM requests over HTTP
 */
class CombinedRequestTracker {
public:
    CombinedRequestTracker(const std::string& method, const std::string& path, 
                          const std::string& llm_type, size_t request_size = 0, size_t max_tokens = 0)
        : http_tracker_(method, path, request_size),
          llm_tracker_(llm_type, max_tokens) {}
    
    // HTTP methods
    void complete_http(int status_code, size_t response_size) {
        http_tracker_.complete(status_code, response_size);
    }
    
    void complete_http_from_context() {
        http_tracker_.complete_from_context();
    }
    
    // LLM methods (delegated)
    void first_token() { llm_tracker_.first_token(); }
    void token_generated() { llm_tracker_.token_generated(); }
    void scheduled() { llm_tracker_.scheduled(); }
    void finish(FinishReason reason = FinishReason::COMPLETED) { 
        llm_tracker_.finish(reason);
    }
    
    const std::string& llm_id() const { return llm_tracker_.id(); }
    
private:
    HTTPRequestTracker http_tracker_;
    kolosal::metrics::LLMRequestTracker llm_tracker_;
};

/**
 * Helper for batch metrics recording
 */
class BatchMetricsRecorder {
public:
    static void record_batch_size(size_t size) {
        LLMMetrics::instance().collector().record_batch_size(size);
    }
    
    static void record_prefill(size_t tokens, double duration_ms) {
        LLMMetrics::instance().collector().record_prefill_batch(tokens, duration_ms);
    }
    
    static void record_decode(size_t tokens, double duration_ms) {
        LLMMetrics::instance().collector().record_decode_batch(tokens, duration_ms);
    }
};

/**
 * Helper for model metrics
 */
class ModelMetricsRecorder {
public:
    // Scoped model load timer
    class LoadTimer {
    public:
        explicit LoadTimer(const std::string& model_name)
            : model_name_(model_name),
              start_(std::chrono::steady_clock::now()) {}
        
        ~LoadTimer() {
            auto duration = std::chrono::steady_clock::now() - start_;
            double seconds = std::chrono::duration<double>(duration).count();
            LLMMetrics::instance().collector().record_model_loaded(model_name_, seconds);
        }
        
    private:
        std::string model_name_;
        std::chrono::steady_clock::time_point start_;
    };
    
    static void model_unloaded(const std::string& model_name) {
        LLMMetrics::instance().collector().record_model_unloaded(model_name);
    }
    
    static void update_cache_usage(double gpu_percent, double cpu_percent = 0.0) {
        LLMMetrics::instance().collector().update_kv_cache_usage(gpu_percent, cpu_percent);
    }
    
    static void update_gpu_memory(size_t used, size_t total) {
        LLMMetrics::instance().collector().update_gpu_memory_usage(used, total);
    }
};

} // namespace metrics
} // namespace kolosal