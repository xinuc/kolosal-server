#include "kolosal/metrics/llm_metrics.hpp"
#include <algorithm>
#include <numeric>

namespace kolosal
{
    namespace metrics
    {

    // RequestMetrics implementation
    double RequestMetrics::time_to_first_token() const {
        if (!first_token_generated) return -1.0;
        return std::chrono::duration<double, std::milli>(
            first_token_time - start_time).count();
    }

    double RequestMetrics::total_latency() const {
        return std::chrono::duration<double, std::milli>(
            end_time - start_time).count();
    }

    double RequestMetrics::queue_latency() const {
        return std::chrono::duration<double, std::milli>(
            queue_time - start_time).count();
    }

    double RequestMetrics::tokens_per_second() const {
        if (generated_tokens == 0) return 0.0;
        double total_time_s = std::chrono::duration<double>(
            end_time - start_time).count();
        return total_time_s > 0.0 ? generated_tokens / total_time_s : 0.0;
    }

    // LLMMetricsCollector implementation
    LLMMetricsCollector::LLMMetricsCollector() {
        // Metrics will be registered on first access via singleton
    }

    void LLMMetricsCollector::register_metrics() {
        auto& registry = MetricRegistry::instance();
        
        // Token metrics
        prompt_tokens_total_ = build()
            .name("kolosal_llm_prompt_tokens_total")
            .help("Total number of prompt tokens processed")
            .counter();
        prompt_tokens_counter_ = prompt_tokens_total_->add();
        
        generation_tokens_total_ = build()
            .name("kolosal_llm_generation_tokens_total")
            .help("Total number of tokens generated")
            .counter();
        generation_tokens_counter_ = generation_tokens_total_->add();
        
        tokens_per_second_ = build()
            .name("kolosal_llm_tokens_per_second")
            .help("Current tokens generation rate per second")
            .gauge();
        tokens_per_second_gauge_ = tokens_per_second_->add();
        
        // Request state metrics
        requests_running_ = build()
            .name("kolosal_llm_requests_running")
            .help("Number of requests currently running")
            .gauge();
        running_requests_gauge_ = requests_running_->add();
        
        requests_waiting_ = build()
            .name("kolosal_llm_requests_waiting")
            .help("Number of requests waiting to be scheduled")
            .gauge();
        waiting_requests_gauge_ = requests_waiting_->add();
        
        requests_swapped_ = build()
            .name("kolosal_llm_requests_swapped")
            .help("Number of requests swapped to CPU")
            .gauge();
        swapped_requests_gauge_ = requests_swapped_->add();
        
        // Request completion metrics
        requests_total_ = build()
            .name("kolosal_llm_requests_total")
            .help("Total number of inference requests")
            .counter();
        auto requests_total_counter = requests_total_->add();
        requests_total_counter->set(0);
        
        request_success_ = build()
            .name("kolosal_llm_request_success_total")
            .help("Total number of successful requests by finish reason")
            .counter();
        // Create default instances for common finish reasons
        request_success_->add({{"reason", "completed"}})->set(0);
        request_success_->add({{"reason", "length"}})->set(0);
        request_success_->add({{"reason", "stop_sequence"}})->set(0);
        
        request_failure_ = build()
            .name("kolosal_llm_request_failure_total")
            .help("Total number of failed requests by reason")
            .counter();
        // Create default instances for common failure reasons
        request_failure_->add({{"reason", "cancelled"}})->set(0);
        request_failure_->add({{"reason", "error"}})->set(0);
        request_failure_->add({{"reason", "timeout"}})->set(0);
        
        // Latency metrics (using gauge for now as our system doesn't support histograms yet)
        time_to_first_token_ = build()
            .name("kolosal_llm_time_to_first_token_seconds")
            .help("Time to first token generation in seconds")
            .gauge();
        auto ttft_gauge = time_to_first_token_->add();
        ttft_gauge->set(0.0);
        
        e2e_request_latency_ = build()
            .name("kolosal_llm_e2e_request_latency_seconds")
            .help("End-to-end request latency in seconds")
            .gauge();
        auto e2e_gauge = e2e_request_latency_->add();
        e2e_gauge->set(0.0);
        
        queue_time_ = build()
            .name("kolosal_llm_request_queue_time_seconds")
            .help("Time spent waiting in queue in seconds")
            .gauge();
        auto queue_gauge = queue_time_->add();
        queue_gauge->set(0.0);
        
        prefill_time_ = build()
            .name("kolosal_llm_prefill_time_seconds")
            .help("Time spent in prefill phase in seconds")
            .gauge();
        auto prefill_gauge = prefill_time_->add();
        prefill_gauge->set(0.0);
        
        decode_time_ = build()
            .name("kolosal_llm_decode_time_seconds")
            .help("Time spent in decode phase in seconds")
            .gauge();
        auto decode_gauge = decode_time_->add();
        decode_gauge->set(0.0);
        
        // Batch metrics
        batch_size_histogram_ = build()
            .name("kolosal_llm_batch_size")
            .help("Distribution of batch sizes")
            .gauge();
        auto batch_size_gauge = batch_size_histogram_->add();
        batch_size_gauge->set(0);
        
        prefill_throughput_ = build()
            .name("kolosal_llm_prefill_tokens_per_second")
            .help("Prefill phase throughput in tokens per second")
            .gauge();
        auto prefill_throughput_gauge = prefill_throughput_->add();
        prefill_throughput_gauge->set(0.0);
        
        decode_throughput_ = build()
            .name("kolosal_llm_decode_tokens_per_second")
            .help("Decode phase throughput in tokens per second")
            .gauge();
        auto decode_throughput_gauge = decode_throughput_->add();
        decode_throughput_gauge->set(0.0);
        
        // Model metrics
        models_loaded_ = build()
            .name("kolosal_llm_models_loaded")
            .help("Number of currently loaded models")
            .gauge();
        auto models_loaded_gauge = models_loaded_->add();
        models_loaded_gauge->set(0);
        
        model_load_time_ = build()
            .name("kolosal_llm_model_load_time_seconds")
            .help("Model loading time in seconds")
            .gauge();
        auto model_load_time_gauge = model_load_time_->add();
        model_load_time_gauge->set(0.0);
        
        // Cache metrics
        gpu_cache_usage_ = build()
            .name("kolosal_llm_gpu_cache_usage_percent")
            .help("GPU KV cache utilization percentage")
            .gauge();
        gpu_cache_gauge_ = gpu_cache_usage_->add();
        
        cpu_cache_usage_ = build()
            .name("kolosal_llm_cpu_cache_usage_percent")
            .help("CPU cache utilization percentage")
            .gauge();
        cpu_cache_gauge_ = cpu_cache_usage_->add();
        
        gpu_memory_usage_ = build()
            .name("kolosal_llm_gpu_memory_used_bytes")
            .help("GPU memory used by LLM inference in bytes")
            .gauge();
        gpu_memory_used_gauge_ = gpu_memory_usage_->add();
        
        gpu_memory_total_ = build()
            .name("kolosal_llm_gpu_memory_total_bytes")
            .help("Total GPU memory available for LLM inference in bytes")
            .gauge();
        gpu_memory_total_gauge_ = gpu_memory_total_->add();
        
        // Engine metrics
        iteration_tokens_ = build()
            .name("kolosal_llm_iteration_tokens_total")
            .help("Total tokens processed per engine iteration")
            .gauge();
        auto iteration_tokens_gauge = iteration_tokens_->add();
        iteration_tokens_gauge->set(0);
        
        iteration_duration_ = build()
            .name("kolosal_llm_iteration_duration_seconds")
            .help("Engine iteration duration in seconds")
            .gauge();
        auto iteration_duration_gauge = iteration_duration_->add();
        iteration_duration_gauge->set(0.0);
    }

    void LLMMetricsCollector::record_request_start(const std::string& request_id, 
                                                   size_t prompt_tokens, 
                                                   size_t max_tokens) {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        auto& request = active_requests_[request_id];
        request.request_id = request_id;
        request.start_time = std::chrono::steady_clock::now();
        request.prompt_tokens = prompt_tokens;
        request.max_tokens = max_tokens;
        
        // Update counters atomically
        const size_t new_prompt_total = total_prompt_tokens_.fetch_add(prompt_tokens) + prompt_tokens;
        prompt_tokens_counter_->set(static_cast<double>(new_prompt_total));
        
        const size_t new_request_total = total_requests_.fetch_add(1) + 1;
        requests_total_->add()->set(static_cast<double>(new_request_total));
    }

    void LLMMetricsCollector::record_request_queued(const std::string& request_id) {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        const auto it = active_requests_.find(request_id);
        if (it != active_requests_.end()) {
            it->second.queue_time = std::chrono::steady_clock::now();
        }
    }

    void LLMMetricsCollector::record_request_scheduled(const std::string& request_id) {
        // Track when request moves from queue to execution
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        const auto it = active_requests_.find(request_id);
        if (it != active_requests_.end()) {
            // Scheduling timestamp could be recorded here if needed for future metrics
        }
    }

    void LLMMetricsCollector::record_first_token(const std::string& request_id) {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        const auto it = active_requests_.find(request_id);
        if (it != active_requests_.end()) {
            it->second.first_token_time = std::chrono::steady_clock::now();
            it->second.first_token_generated = true;
            
            // Record TTFT metric in seconds
            const double ttft_seconds = it->second.time_to_first_token() / 1000.0;
            time_to_first_token_->add()->set(ttft_seconds);
        }
    }

    void LLMMetricsCollector::record_token_generated(const std::string& request_id) {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        const auto it = active_requests_.find(request_id);
        if (it != active_requests_.end()) {
            it->second.generated_tokens++;
            
            // Update global counter atomically
            const size_t new_token_total = total_generated_tokens_.fetch_add(1) + 1;
            generation_tokens_counter_->set(static_cast<double>(new_token_total));
        }
    }

    void LLMMetricsCollector::record_request_finished(const std::string& request_id, 
                                                      FinishReason reason) {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        const auto it = active_requests_.find(request_id);
        if (it != active_requests_.end()) {
            it->second.end_time = std::chrono::steady_clock::now();
            it->second.finish_reason = reason;
            
            // Record latency metrics in seconds
            const double total_latency_s = it->second.total_latency() / 1000.0;
            e2e_request_latency_->add()->set(total_latency_s);
            
            if (it->second.queue_time != std::chrono::steady_clock::time_point{}) {
                const double queue_latency_s = it->second.queue_latency() / 1000.0;
                queue_time_->add()->set(queue_latency_s);
            }
            
            // Update success/failure counters
            const std::string reason_str = reason_to_string(reason);
            if (reason == FinishReason::COMPLETED || 
                reason == FinishReason::STOP_SEQUENCE || 
                reason == FinishReason::LENGTH) {
                const size_t new_success_count = successful_requests_.fetch_add(1) + 1;
                request_success_->add({{"reason", reason_str}})->set(
                    static_cast<double>(new_success_count)
                );
            } else {
                const size_t new_failure_count = failed_requests_.fetch_add(1) + 1;
                request_failure_->add({{"reason", reason_str}})->set(
                    static_cast<double>(new_failure_count)
                );
            }
            
            // Update tokens per second gauge
            const double tps = it->second.tokens_per_second();
            if (tps > 0.0) {
                tokens_per_second_gauge_->set(tps);
            }
            
            // Move to completed requests for statistics
            completed_requests_.push_back(it->second);
            active_requests_.erase(it);
            
            // Keep only recent completed requests (last 1000) to prevent unbounded growth
            static constexpr size_t MAX_COMPLETED_REQUESTS = 1000;
            if (completed_requests_.size() > MAX_COMPLETED_REQUESTS) {
                completed_requests_.erase(completed_requests_.begin());
            }
        }
    }

    void LLMMetricsCollector::record_batch_size(size_t batch_size) {
        batch_size_histogram_->add()->set(static_cast<double>(batch_size));
    }

    void LLMMetricsCollector::record_prefill_batch(size_t num_tokens, double duration_ms) {
        double throughput = duration_ms > 0.0 ? 
            (num_tokens * 1000.0) / duration_ms : 0.0;
        prefill_throughput_->add()->set(throughput);
        
        double duration_s = duration_ms / 1000.0;
        prefill_time_->add()->set(duration_s);
    }

    void LLMMetricsCollector::record_decode_batch(size_t num_tokens, double duration_ms) {
        double throughput = duration_ms > 0.0 ? 
            (num_tokens * 1000.0) / duration_ms : 0.0;
        decode_throughput_->add()->set(throughput);
        
        double duration_s = duration_ms / 1000.0;
        decode_time_->add()->set(duration_s);
    }

    void LLMMetricsCollector::record_model_loaded(const std::string& model_name, 
                                                  double load_time_seconds) {
        model_load_time_->add({{"model", model_name}})->set(load_time_seconds);
        
        // Update loaded models count (simplified - would need proper tracking)
        models_loaded_->add()->set(1.0);
    }

    void LLMMetricsCollector::record_model_unloaded(const std::string& model_name) {
        // Update loaded models count (simplified - would need proper tracking)
        models_loaded_->add()->set(0.0);
    }

    void LLMMetricsCollector::update_kv_cache_usage(double gpu_cache_percent, 
                                                    double cpu_cache_percent) {
        gpu_cache_gauge_->set(gpu_cache_percent);
        cpu_cache_gauge_->set(cpu_cache_percent);
    }

    void LLMMetricsCollector::update_gpu_memory_usage(size_t used_bytes, size_t total_bytes) {
        gpu_memory_used_gauge_->set(static_cast<double>(used_bytes));
        gpu_memory_total_gauge_->set(static_cast<double>(total_bytes));
    }

    void LLMMetricsCollector::record_iteration(size_t tokens_processed, double duration_ms) {
        iteration_tokens_->add()->set(static_cast<double>(tokens_processed));
        iteration_duration_->add()->set(duration_ms / 1000.0);
    }

    void LLMMetricsCollector::update_active_requests(size_t running, 
                                                     size_t waiting, 
                                                     size_t swapped) {
        running_requests_gauge_->set(static_cast<double>(running));
        waiting_requests_gauge_->set(static_cast<double>(waiting));
        swapped_requests_gauge_->set(static_cast<double>(swapped));
    }

    LLMMetricsCollector::Statistics LLMMetricsCollector::get_statistics() const {
        const std::lock_guard<std::mutex> lock(requests_mutex_);
        
        Statistics stats;
        stats.total_requests = total_requests_.load();
        stats.active_requests = active_requests_.size();
        stats.completed_requests = successful_requests_.load();
        stats.failed_requests = failed_requests_.load();
        
        if (!completed_requests_.empty()) {
            // Calculate averages from recent completed requests
            std::vector<double> ttft_times, tps_values, latencies;
            ttft_times.reserve(completed_requests_.size());
            tps_values.reserve(completed_requests_.size());
            latencies.reserve(completed_requests_.size());
            
            for (const auto& req : completed_requests_) {
                if (req.first_token_generated) {
                    ttft_times.push_back(req.time_to_first_token());
                }
                tps_values.push_back(req.tokens_per_second());
                latencies.push_back(req.total_latency());
            }
            
            if (!ttft_times.empty()) {
                stats.avg_time_to_first_token_ms = 
                    std::accumulate(ttft_times.begin(), ttft_times.end(), 0.0) / static_cast<double>(ttft_times.size());
            }
            
            if (!tps_values.empty()) {
                stats.avg_tokens_per_second = 
                    std::accumulate(tps_values.begin(), tps_values.end(), 0.0) / static_cast<double>(tps_values.size());
            }
            
            if (!latencies.empty()) {
                std::sort(latencies.begin(), latencies.end());
                const size_t size = latencies.size();
                stats.p50_latency_ms = latencies[size * 50 / 100];
                stats.p95_latency_ms = latencies[size * 95 / 100];
                stats.p99_latency_ms = latencies[size * 99 / 100];
            }
        }
        
        return stats;
    }

    std::string LLMMetricsCollector::reason_to_string(FinishReason reason) const {
        switch (reason) {
            case FinishReason::COMPLETED: return "completed";
            case FinishReason::LENGTH: return "length";
            case FinishReason::STOP_SEQUENCE: return "stop_sequence";
            case FinishReason::CANCELLED: return "cancelled";
            case FinishReason::ERROR: return "error";
            case FinishReason::TIMEOUT: return "timeout";
            default: return "unknown";
        }
    }

    } // namespace metrics
} // namespace kolosal