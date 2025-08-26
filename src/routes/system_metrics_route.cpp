#include "kolosal/routes/system_metrics_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/metrics/system_metrics.hpp"
#include "kolosal/metrics/llm_metrics.hpp"
#include "kolosal/metrics/http_metrics.hpp"
#include "kolosal/metrics/formatters.hpp"
#include <json.hpp>
#include <thread>
#include <memory>
#include <mutex>

using json = nlohmann::json;

namespace kolosal
{

    bool SystemMetricsRoute::match(const std::string &method, const std::string &path)
    {
        if ((method == "GET" || method == "OPTIONS") && 
            (path == "/metrics" || path == "/v1/metrics" || path == "/system-metrics"))
        {
            current_method_ = method;
            return true;
        }
        return false;
    }

    void SystemMetricsRoute::handle(SocketType sock, const std::string &body)
    {        
        try
        {
            // Handle OPTIONS request for CORS preflight
            if (current_method_ == "OPTIONS")
            {
                ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for metrics endpoint", 
                                       std::this_thread::get_id());
                
                std::map<std::string, std::string> headers = {
                    {"Content-Type", "text/plain"},
                    {"Access-Control-Allow-Origin", "*"},
                    {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                    {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                    {"Access-Control-Max-Age", "86400"} // Cache preflight for 24 hours
                };
                
                send_response(sock, 200, "", headers);
                return;
            }

            ServerLogger::logDebug("[Thread %u] Received system metrics request", std::this_thread::get_id());

            // Initialize metrics only once using singleton pattern
            static metrics::SystemMetricsCollector system_collector;
            system_collector.collect();
            
            // Initialize LLM metrics (singleton - only creates metrics once)
            auto& llm_metrics = metrics::LLMMetrics::instance().collector();
            // LLM metrics are updated during request processing
            
            // Initialize HTTP metrics (singleton - only creates metrics once)
            static std::once_flag http_metrics_flag;
            std::call_once(http_metrics_flag, []() {
                metrics::HTTPMetricsCollector::instance().register_metrics();
            });
            
            // Add engine metrics (but prevent duplication)
            static std::once_flag engine_metrics_flag;
            std::call_once(engine_metrics_flag, [this]() {
                add_engine_metrics();
            });
            
            // Format response based on endpoint
            std::unique_ptr<metrics::MetricFormatter> formatter;
            
            // TODO: Parse Accept header from request to determine format
            // For now, always use Prometheus format
            bool usePrometheus = true;
            
            if (usePrometheus) {
                formatter = std::make_unique<metrics::PrometheusFormatter>();
            } else {
                formatter = std::make_unique<metrics::JsonFormatter>();
            }
            
            auto& registry = metrics::MetricRegistry::instance();
            std::string responseBody = formatter->format(registry);
            std::string contentType = formatter->content_type();

            std::map<std::string, std::string> headers = {
                {"Content-Type", contentType},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
            };

            send_response(sock, 200, responseBody, headers);
            ServerLogger::logDebug("[Thread %u] Successfully provided system metrics", std::this_thread::get_id());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling system metrics request: %s", 
                                   std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {
                    {"message", std::string("Server error: ") + ex.what()}, 
                    {"type", "server_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };

            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
            };

            send_response(sock, 500, jError.dump(), headers);
        }
    }
    
    void SystemMetricsRoute::add_engine_metrics()
    {
        try {
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            auto engineIds = nodeManager.listEngineIds();
            
            auto& registry = metrics::MetricRegistry::instance();
            
            auto engines_total = metrics::build()
                .name("kolosal_engines_total")
                .help("Total number of engines")
                .gauge();
            engines_total->add()->set(engineIds.size());
            
            int loadedCount = 0;
            for (const auto &engineId : engineIds) {
                auto [exists, isLoaded] = nodeManager.getEngineStatus(engineId);
                if (isLoaded) loadedCount++;
            }
            
            auto engines_loaded = metrics::build()
                .name("kolosal_engines_loaded")
                .help("Number of loaded engines")
                .gauge();
            engines_loaded->add()->set(loadedCount);
            
        } catch (const std::exception &ex) {
            ServerLogger::logWarning("Failed to get engine metrics: %s", ex.what());
        }
    }


} // namespace kolosal