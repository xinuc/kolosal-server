#ifndef KOLOSAL_LLM_METRICS_HPP
#define KOLOSAL_LLM_METRICS_HPP

#include "metric_registry.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace kolosal
{
    namespace metrics
    {

    /// Enum for request completion reasons
    enum class FinishReason {
        COMPLETED,      ///< Normal completion
        LENGTH,         ///< Max tokens reached
        STOP_SEQUENCE,  ///< Stop sequence encountered
        CANCELLED,      ///< User cancelled
        ERROR,          ///< Error occurred
        TIMEOUT         ///< Request timed out
    };

    /// Request tracking structure for individual request metrics
    struct RequestMetrics {
        std::string request_id;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point queue_time;
        std::chrono::steady_clock::time_point first_token_time;
        std::chrono::steady_clock::time_point end_time;
        
        size_t prompt_tokens = 0;
        size_t generated_tokens = 0;
        size_t max_tokens = 0;
        
        bool first_token_generated = false;
        FinishReason finish_reason = FinishReason::COMPLETED;
        
        /// Calculate time to first token in milliseconds
        double time_to_first_token() const;
        
        /// Calculate total request latency in milliseconds
        double total_latency() const;
        
        /// Calculate queue latency in milliseconds
        double queue_latency() const;
        
        /// Calculate tokens per second generation rate
        double tokens_per_second() const;
    };

    /// Collects and manages LLM-specific metrics
    class LLMMetricsCollector {
    public:
        LLMMetricsCollector();
        ~LLMMetricsCollector() = default;
        
        /// Initialize and register all LLM metrics with the registry
        void register_metrics();
        
        /// @name Request Lifecycle Tracking
        /// @{
        void record_request_start(const std::string& request_id, size_t prompt_tokens, size_t max_tokens);
        void record_request_queued(const std::string& request_id);
        void record_request_scheduled(const std::string& request_id);
        void record_first_token(const std::string& request_id);
        void record_token_generated(const std::string& request_id);
        void record_request_finished(const std::string& request_id, FinishReason reason);
        /// @}
        
        /// @name Batch Processing Metrics
        /// @{
        void record_batch_size(size_t batch_size);
        void record_prefill_batch(size_t num_tokens, double duration_ms);
        void record_decode_batch(size_t num_tokens, double duration_ms);
        /// @}
        
        /// @name Model and Cache Metrics
        /// @{
        void record_model_loaded(const std::string& model_name, double load_time_seconds);
        void record_model_unloaded(const std::string& model_name);
        void update_kv_cache_usage(double gpu_cache_percent, double cpu_cache_percent = 0.0);
        void update_gpu_memory_usage(size_t used_bytes, size_t total_bytes);
        /// @}
        
        /// @name Engine Performance Metrics
        /// @{
        void record_iteration(size_t tokens_processed, double duration_ms);
        void update_active_requests(size_t running, size_t waiting, size_t swapped = 0);
        /// @}
        
        /// Statistics structure for current metrics snapshot
        struct Statistics {
            size_t total_requests = 0;
            size_t active_requests = 0;
            size_t completed_requests = 0;
            size_t failed_requests = 0;
            double avg_time_to_first_token_ms = 0.0;
            double avg_tokens_per_second = 0.0;
            double p50_latency_ms = 0.0;
            double p95_latency_ms = 0.0;
            double p99_latency_ms = 0.0;
        };
        
        /// Get current statistics snapshot
        Statistics get_statistics() const;
        
    private:
        /// @name Token Metrics
        /// @{
        std::shared_ptr<MetricFamily> prompt_tokens_total_;
        std::shared_ptr<MetricFamily> generation_tokens_total_;
        std::shared_ptr<MetricFamily> tokens_per_second_;
        /// @}
        
        /// @name Latency Metrics
        /// @{
        std::shared_ptr<MetricFamily> time_to_first_token_;
        std::shared_ptr<MetricFamily> inter_token_latency_;
        std::shared_ptr<MetricFamily> e2e_request_latency_;
        std::shared_ptr<MetricFamily> queue_time_;
        std::shared_ptr<MetricFamily> prefill_time_;
        std::shared_ptr<MetricFamily> decode_time_;
        /// @}
        
        /// @name Request State Metrics
        /// @{
        std::shared_ptr<MetricFamily> requests_total_;
        std::shared_ptr<MetricFamily> requests_running_;
        std::shared_ptr<MetricFamily> requests_waiting_;
        std::shared_ptr<MetricFamily> requests_swapped_;
        std::shared_ptr<MetricFamily> request_success_;
        std::shared_ptr<MetricFamily> request_failure_;
        /// @}
        
        /// @name Batch Processing Metrics
        /// @{
        std::shared_ptr<MetricFamily> batch_size_histogram_;
        std::shared_ptr<MetricFamily> prefill_throughput_;
        std::shared_ptr<MetricFamily> decode_throughput_;
        /// @}
        
        /// @name Model Metrics
        /// @{
        std::shared_ptr<MetricFamily> models_loaded_;
        std::shared_ptr<MetricFamily> model_load_time_;
        /// @}
        
        /// @name Cache and Memory Metrics
        /// @{
        std::shared_ptr<MetricFamily> gpu_cache_usage_;
        std::shared_ptr<MetricFamily> cpu_cache_usage_;
        std::shared_ptr<MetricFamily> gpu_memory_usage_;
        std::shared_ptr<MetricFamily> gpu_memory_total_;
        /// @}
        
        /// @name Engine Iteration Metrics
        /// @{
        std::shared_ptr<MetricFamily> iteration_tokens_;
        std::shared_ptr<MetricFamily> iteration_duration_;
        /// @}
        
        /// @name Cached Metric Instances
        /// @{
        std::shared_ptr<Metric> prompt_tokens_counter_;
        std::shared_ptr<Metric> generation_tokens_counter_;
        std::shared_ptr<Metric> tokens_per_second_gauge_;
        
        std::shared_ptr<Metric> running_requests_gauge_;
        std::shared_ptr<Metric> waiting_requests_gauge_;
        std::shared_ptr<Metric> swapped_requests_gauge_;
        
        std::shared_ptr<Metric> gpu_cache_gauge_;
        std::shared_ptr<Metric> cpu_cache_gauge_;
        std::shared_ptr<Metric> gpu_memory_used_gauge_;
        std::shared_ptr<Metric> gpu_memory_total_gauge_;
        /// @}
        
        /// @name Request Tracking State
        /// @{
        mutable std::mutex requests_mutex_;
        std::unordered_map<std::string, RequestMetrics> active_requests_;
        std::vector<RequestMetrics> completed_requests_;
        /// @}
        
        /// @name Statistics Counters
        /// @{
        std::atomic<size_t> total_prompt_tokens_{0};
        std::atomic<size_t> total_generated_tokens_{0};
        std::atomic<size_t> total_requests_{0};
        std::atomic<size_t> successful_requests_{0};
        std::atomic<size_t> failed_requests_{0};
        /// @}
        
        /// @name Helper Methods
        /// @{
        std::string reason_to_string(FinishReason reason) const;
        /// @}
    };

    /// Global LLM metrics instance (singleton pattern)
    class LLMMetrics {
    public:
        /// Get the singleton instance
        static LLMMetrics& instance() {
            static LLMMetrics instance;
            return instance;
        }
        
        /// Get the metrics collector instance
        LLMMetricsCollector& collector() { 
            std::call_once(init_flag_, [this]() {
                collector_.register_metrics();
            });
            return collector_; 
        }
        
        // Delete copy/move constructors and assignments
        LLMMetrics(const LLMMetrics&) = delete;
        LLMMetrics& operator=(const LLMMetrics&) = delete;
        LLMMetrics(LLMMetrics&&) = delete;
        LLMMetrics& operator=(LLMMetrics&&) = delete;
        
    private:
        LLMMetrics() = default;
        ~LLMMetrics() = default;
        
        LLMMetricsCollector collector_;
        std::once_flag init_flag_;
    };

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_LLM_METRICS_HPP