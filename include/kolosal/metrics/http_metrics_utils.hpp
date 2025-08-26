#ifndef KOLOSAL_HTTP_METRICS_UTILS_HPP
#define KOLOSAL_HTTP_METRICS_UTILS_HPP

#include "../utils.hpp"
#include "http_metrics.hpp"

namespace kolosal {
namespace metrics {

// Wrapper for send_response that records metrics
inline void send_response_with_metrics(
    SocketType sock,
    int status_code,
    const std::string& body,
    const std::map<std::string, std::string>& headers = { {"Content-Type", "application/json"} }) {
    
    // Send the response
    send_response(sock, status_code, body, headers);
    
    // Record HTTP metrics if context is set
    auto& context = kolosal::http_internal::g_request_context;
    if (context.metrics_enabled && !context.method.empty()) {
        HTTPMetricsCollector::instance().record_request_complete(
            context.method,
            context.path,
            status_code,
            context.request_size,
            body.size(),
            context.start_time
        );
        context.metrics_enabled = false; // Prevent double recording
    }
}

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_HTTP_METRICS_UTILS_HPP