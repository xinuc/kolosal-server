#ifndef KOLOSAL_HTTP_METRICS_HPP
#define KOLOSAL_HTTP_METRICS_HPP

#include "metric_types.hpp"
#include "metric_registry.hpp"
#include <chrono>
#include <string>
#include <memory>
#include <map>
#include <mutex>

namespace kolosal {
namespace metrics {

    class HTTPMetricsCollector {
    public:
        // Singleton pattern for consistent metric collection
        static HTTPMetricsCollector& instance() {
            static HTTPMetricsCollector collector;
            return collector;
        }
        
        // Record the start of an HTTP request
        void record_request_start(const std::string& method, const std::string& path);
        
        // Record the completion of an HTTP request
        void record_request_complete(const std::string& method, 
                                    const std::string& path,
                                    int status_code,
                                    size_t request_size_bytes,
                                    size_t response_size_bytes,
                                    std::chrono::steady_clock::time_point start_time);
        
        // Get current number of in-flight requests
        size_t get_in_flight_requests() const { return in_flight_requests_; }
        
        // Initialize metrics (called once)
        void register_metrics();
        
    private:
        HTTPMetricsCollector();
        HTTPMetricsCollector(const HTTPMetricsCollector&) = delete;
        HTTPMetricsCollector& operator=(const HTTPMetricsCollector&) = delete;
        
        // Metric families
        std::shared_ptr<MetricFamily> http_requests_total_;
        std::shared_ptr<MetricFamily> http_request_duration_seconds_;
        std::shared_ptr<MetricFamily> http_request_size_bytes_;
        std::shared_ptr<MetricFamily> http_response_size_bytes_;
        std::shared_ptr<MetricFamily> http_requests_in_flight_;
        
        // Cached metrics for performance
        std::map<std::string, std::shared_ptr<Metric>> request_counters_;
        std::map<std::string, std::shared_ptr<Metric>> duration_histograms_;
        std::map<std::string, std::shared_ptr<Metric>> request_size_histograms_;
        std::map<std::string, std::shared_ptr<Metric>> response_size_histograms_;
        std::shared_ptr<Metric> in_flight_gauge_;
        
        // Thread safety
        mutable std::mutex metrics_mutex_;
        std::atomic<size_t> in_flight_requests_{0};
        
        // Helper to get or create labeled metric
        std::shared_ptr<Metric> get_or_create_metric(
            std::shared_ptr<MetricFamily>& family,
            std::map<std::string, std::shared_ptr<Metric>>& cache,
            const std::string& cache_key,
            const std::map<std::string, std::string>& labels);
        
        // Normalize path for metric labels (avoid high cardinality)
        std::string normalize_path(const std::string& path) const;
        
        // Create cache key from labels
        std::string create_cache_key(const std::string& method, 
                                    const std::string& path, 
                                    int status_code = -1) const;
    };

    // RAII helper for tracking request lifecycle
    class HTTPRequestTracker {
    public:
        HTTPRequestTracker(const std::string& method, const std::string& path)
            : method_(method), path_(path), start_time_(std::chrono::steady_clock::now()) {
            HTTPMetricsCollector::instance().record_request_start(method, path);
        }
        
        ~HTTPRequestTracker() {
            // If not explicitly completed, record as error
            if (!completed_) {
                complete(500, 0, 0);
            }
        }
        
        void complete(int status_code, size_t request_size, size_t response_size) {
            if (!completed_) {
                HTTPMetricsCollector::instance().record_request_complete(
                    method_, path_, status_code, request_size, response_size, start_time_);
                completed_ = true;
            }
        }
        
    private:
        std::string method_;
        std::string path_;
        std::chrono::steady_clock::time_point start_time_;
        bool completed_ = false;
    };

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_HTTP_METRICS_HPP