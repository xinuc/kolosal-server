#include "kolosal/metrics/http_metrics.hpp"
#include <sstream>
#include <regex>

namespace kolosal {
namespace metrics {

    HTTPMetricsCollector::HTTPMetricsCollector() {
        // Register metrics immediately on construction
        register_metrics();
    }

    void HTTPMetricsCollector::register_metrics() {
        static bool registered = false;
        if (registered) return;
        registered = true;
        
        auto& registry = MetricRegistry::instance();
        
        // HTTP request counter
        http_requests_total_ = build()
            .name("kolosal_http_requests_total")
            .help("Total number of HTTP requests by method, path and status code")
            .counter();
        
        // HTTP request duration histogram
        http_request_duration_seconds_ = build()
            .name("kolosal_http_request_duration_seconds")
            .help("HTTP request duration in seconds")
            .histogram();
        
        // HTTP request size histogram
        http_request_size_bytes_ = build()
            .name("kolosal_http_request_size_bytes")
            .help("HTTP request body size in bytes")
            .histogram();
        
        // HTTP response size histogram
        http_response_size_bytes_ = build()
            .name("kolosal_http_response_size_bytes")
            .help("HTTP response body size in bytes")
            .histogram();
        
        // HTTP requests in flight gauge
        http_requests_in_flight_ = build()
            .name("kolosal_http_requests_in_flight")
            .help("Number of HTTP requests currently being processed")
            .gauge();
        
        // Initialize the in-flight gauge
        in_flight_gauge_ = http_requests_in_flight_->add();
        in_flight_gauge_->set(0.0);
        
        // Note: We don't pre-create metric instances anymore to avoid clutter
        // Metrics will be created on first use
    }

    void HTTPMetricsCollector::record_request_start(const std::string& method, 
                                                   const std::string& path) {
        // Make sure metrics are initialized
        if (!in_flight_gauge_) {
            register_metrics();
        }
        
        // Increment in-flight requests
        size_t current = in_flight_requests_.fetch_add(1) + 1;
        
        // Update gauge
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (in_flight_gauge_) {
            in_flight_gauge_->set(static_cast<double>(current));
        }
    }

    void HTTPMetricsCollector::record_request_complete(const std::string& method,
                                                      const std::string& path,
                                                      int status_code,
                                                      size_t request_size_bytes,
                                                      size_t response_size_bytes,
                                                      std::chrono::steady_clock::time_point start_time) {
        // Make sure metrics are initialized
        if (!http_requests_total_) {
            register_metrics();
        }
        
        // Calculate request duration
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration_seconds = end_time - start_time;
        
        // Normalize path to avoid high cardinality
        std::string normalized_path = normalize_path(path);
        
        // Create labels
        std::map<std::string, std::string> labels = {
            {"method", method},
            {"path", normalized_path},
            {"status", std::to_string(status_code)}
        };
        
        std::map<std::string, std::string> size_labels = {
            {"method", method},
            {"path", normalized_path}
        };
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        // Update request counter
        std::string counter_key = create_cache_key(method, normalized_path, status_code);
        auto counter = get_or_create_metric(http_requests_total_, 
                                           request_counters_, 
                                           counter_key, 
                                           labels);
        if (auto* c = dynamic_cast<Counter*>(counter.get())) {
            c->increment();
        }
        
        // Update duration histogram
        std::string duration_key = create_cache_key(method, normalized_path, status_code);
        auto duration_hist = get_or_create_metric(http_request_duration_seconds_,
                                                 duration_histograms_,
                                                 duration_key,
                                                 labels);
        if (auto* h = dynamic_cast<Histogram*>(duration_hist.get())) {
            h->observe(duration_seconds.count());
        }
        
        // Update request size histogram
        if (request_size_bytes > 0) {
            std::string req_size_key = create_cache_key(method, normalized_path);
            auto req_size_hist = get_or_create_metric(http_request_size_bytes_,
                                                     request_size_histograms_,
                                                     req_size_key,
                                                     size_labels);
            if (auto* h = dynamic_cast<Histogram*>(req_size_hist.get())) {
                h->observe(static_cast<double>(request_size_bytes));
            }
        }
        
        // Update response size histogram
        if (response_size_bytes > 0) {
            std::string resp_size_key = create_cache_key(method, normalized_path);
            auto resp_size_hist = get_or_create_metric(http_response_size_bytes_,
                                                      response_size_histograms_,
                                                      resp_size_key,
                                                      size_labels);
            if (auto* h = dynamic_cast<Histogram*>(resp_size_hist.get())) {
                h->observe(static_cast<double>(response_size_bytes));
            }
        }
        
        // Decrement in-flight requests
        size_t current = in_flight_requests_.fetch_sub(1) - 1;
        if (in_flight_gauge_) {
            in_flight_gauge_->set(static_cast<double>(current));
        }
    }

    std::shared_ptr<Metric> HTTPMetricsCollector::get_or_create_metric(
        std::shared_ptr<MetricFamily>& family,
        std::map<std::string, std::shared_ptr<Metric>>& cache,
        const std::string& cache_key,
        const std::map<std::string, std::string>& labels) {
        
        auto it = cache.find(cache_key);
        if (it != cache.end()) {
            return it->second;
        }
        
        auto metric = family->add(labels);
        cache[cache_key] = metric;
        return metric;
    }

    std::string HTTPMetricsCollector::normalize_path(const std::string& path) const {
        // Common path normalizations to reduce cardinality
        
        // Remove query parameters
        size_t query_pos = path.find('?');
        std::string normalized = (query_pos != std::string::npos) ? 
                                path.substr(0, query_pos) : path;
        
        // Replace numeric IDs with placeholders
        // Pattern: /something/123/something -> /something/{id}/something
        std::regex id_pattern(R"(/\d+)");
        normalized = std::regex_replace(normalized, id_pattern, "/{id}");
        
        // Replace UUIDs with placeholders
        std::regex uuid_pattern(R"(/[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12})");
        normalized = std::regex_replace(normalized, uuid_pattern, "/{uuid}");
        
        // Common API endpoints normalization
        if (normalized.find("/v1/engines/") == 0 || normalized.find("/v1/models/") == 0) {
            // Keep the specific endpoint pattern
            size_t third_slash = normalized.find('/', 12);  // After /v1/engines/
            if (third_slash != std::string::npos) {
                std::string prefix = normalized.substr(0, third_slash);
                normalized = prefix + "/{id}";
            }
        }
        
        // Limit path length to prevent excessive unique paths
        if (normalized.length() > 100) {
            normalized = normalized.substr(0, 100) + "...";
        }
        
        return normalized;
    }

    std::string HTTPMetricsCollector::create_cache_key(const std::string& method,
                                                      const std::string& path,
                                                      int status_code) const {
        std::ostringstream key;
        key << method << "|" << path;
        if (status_code != -1) {
            key << "|" << status_code;
        }
        return key.str();
    }

} // namespace metrics
} // namespace kolosal