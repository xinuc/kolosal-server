#ifndef KOLOSAL_LLM_METRICS_INTEGRATION_HPP
#define KOLOSAL_LLM_METRICS_INTEGRATION_HPP

#include "llm_metrics.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace kolosal
{
    namespace metrics
    {

    /// RAII helper for automatic request lifecycle tracking
    class RequestTracker {
    public:
        RequestTracker(const std::string& request_id, 
                      size_t prompt_tokens, 
                      size_t max_tokens = 0)
            : request_id_(request_id), 
              collector_(LLMMetrics::instance().collector()) {
            collector_.record_request_start(request_id_, prompt_tokens, max_tokens);
        }
        
        ~RequestTracker() {
            if (!finished_) {
                // If request wasn't explicitly finished, mark as error
                finish(FinishReason::ERROR);
            }
        }
        
        // Delete copy constructor and assignment operator
        RequestTracker(const RequestTracker&) = delete;
        RequestTracker& operator=(const RequestTracker&) = delete;
        
        // Move constructor and assignment
        RequestTracker(RequestTracker&& other) noexcept
            : request_id_(std::move(other.request_id_))
            , collector_(other.collector_)
            , finished_(other.finished_) {
            other.finished_ = true; // Prevent double-finish
        }
        
        RequestTracker& operator=(RequestTracker&& other) noexcept {
            if (this != &other) {
                if (!finished_) {
                    finish(FinishReason::ERROR);
                }
                request_id_ = std::move(other.request_id_);
                finished_ = other.finished_;
                other.finished_ = true;
            }
            return *this;
        }
        
        /// Mark request as queued
        void queued() {
            collector_.record_request_queued(request_id_);
        }
        
        /// Mark request as scheduled for execution
        void scheduled() {
            collector_.record_request_scheduled(request_id_);
        }
        
        /// Record first token generation
        void first_token_generated() {
            collector_.record_first_token(request_id_);
        }
        
        /// Record subsequent token generation
        void token_generated() {
            collector_.record_token_generated(request_id_);
        }
        
        /// Finish request with specified reason
        void finish(FinishReason reason) {
            if (!finished_) {
                collector_.record_request_finished(request_id_, reason);
                finished_ = true;
            }
        }
        
        /// Get the request ID
        const std::string& request_id() const { 
            return request_id_; 
        }
        
    private:
        std::string request_id_;
        LLMMetricsCollector& collector_;
        bool finished_ = false;
    };

    /// Helper class for batch processing metrics tracking
    class BatchTracker {
    public:
        explicit BatchTracker(size_t batch_size) 
            : collector_(LLMMetrics::instance().collector())
            , start_time_(std::chrono::steady_clock::now()) {
            collector_.record_batch_size(batch_size);
        }
        
        /// Record prefill phase completion
        void record_prefill_phase(size_t num_tokens) {
            const auto now = std::chrono::steady_clock::now();
            const auto duration = std::chrono::duration<double, std::milli>(now - start_time_).count();
            collector_.record_prefill_batch(num_tokens, duration);
        }
        
        /// Record decode phase completion
        void record_decode_phase(size_t num_tokens) {
            const auto now = std::chrono::steady_clock::now();
            const auto duration = std::chrono::duration<double, std::milli>(now - start_time_).count();
            collector_.record_decode_batch(num_tokens, duration);
        }
        
    private:
        LLMMetricsCollector& collector_;
        std::chrono::steady_clock::time_point start_time_;
    };

    /// @name Convenience Functions for Common Metrics Operations
    /// @{
    
    /// Update GPU KV cache utilization
    inline void update_gpu_kv_cache(double gpu_percent, double cpu_percent = 0.0) {
        LLMMetrics::instance().collector().update_kv_cache_usage(gpu_percent, cpu_percent);
    }
    
    /// Update GPU memory usage
    inline void update_gpu_memory(size_t used_bytes, size_t total_bytes) {
        LLMMetrics::instance().collector().update_gpu_memory_usage(used_bytes, total_bytes);
    }
    
    /// Update active request counts
    inline void update_request_counts(size_t running, size_t waiting, size_t swapped = 0) {
        LLMMetrics::instance().collector().update_active_requests(running, waiting, swapped);
    }
    
    /// Record model loading event
    inline void record_model_loading(const std::string& model_name, double load_time_seconds) {
        LLMMetrics::instance().collector().record_model_loaded(model_name, load_time_seconds);
    }
    
    /// Record model unloading event
    inline void record_model_unloading(const std::string& model_name) {
        LLMMetrics::instance().collector().record_model_unloaded(model_name);
    }
    
    /// Record engine iteration performance
    inline void record_engine_iteration(size_t tokens_processed, double duration_ms) {
        LLMMetrics::instance().collector().record_iteration(tokens_processed, duration_ms);
    }
    
    /// @}

    /// Get current LLM statistics snapshot
    inline LLMMetricsCollector::Statistics get_llm_statistics() {
        return LLMMetrics::instance().collector().get_statistics();
    }

    } // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_LLM_METRICS_INTEGRATION_HPP