#pragma once

#include "route_interface.hpp"
#include "../metrics/http_metrics.hpp"
#include "../utils.hpp"

class BaseRoute : public IRoute {
protected:
    // Wrapper for send_response that automatically records metrics
    void send_response_with_metrics(SocketType sock, int status_code, 
                                   const std::string& body,
                                   const std::map<std::string, std::string>& headers = {{"Content-Type", "application/json"}}) {
        // Send the actual response
        send_response(sock, status_code, body, headers);
        
        // Record metrics
        auto& context = kolosal::http_internal::g_request_context;
        if (context.metrics_enabled && !context.method.empty()) {
            kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
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
};