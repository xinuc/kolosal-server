#include "kolosal/metrics/http_metrics.hpp"
#include "kolosal/utils.hpp"

namespace kolosal {
namespace metrics {

// Static initialization to ensure metrics are registered
static bool ensure_http_metrics_initialized() {
    static bool initialized = false;
    if (!initialized) {
        HTTPMetricsCollector::instance().register_metrics();
        initialized = true;
    }
    return initialized;
}

// Called automatically when any route sends a response
void record_http_response(int status_code, size_t response_size) {
    ensure_http_metrics_initialized();
    
    auto& context = kolosal::http_internal::g_request_context;
    if (!context.method.empty() && context.metrics_enabled) {
        HTTPMetricsCollector::instance().record_request_complete(
            context.method,
            context.path,
            status_code,
            context.request_size,
            response_size,
            context.start_time
        );
    }
}

} // namespace metrics
} // namespace kolosal

// Hook into send_response to automatically record metrics
namespace {
    struct HTTPMetricsHook {
        HTTPMetricsHook() {
            // Ensure metrics are initialized on program start
            kolosal::metrics::ensure_http_metrics_initialized();
        }
    } http_metrics_hook;
}