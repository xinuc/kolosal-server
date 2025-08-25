#ifndef KOLOSAL_SYSTEM_METRICS_HPP
#define KOLOSAL_SYSTEM_METRICS_HPP

#include "metric_registry.hpp"
#include <memory>

namespace kolosal
{
    namespace metrics
    {

    class SystemMetricsCollector {
    public:
        SystemMetricsCollector();
        ~SystemMetricsCollector() = default;
        
        void collect();
        void register_metrics();
        
    private:
        // System info
        std::shared_ptr<MetricFamily> server_info_;
        
        // Memory metrics
        std::shared_ptr<MetricFamily> memory_bytes_;
        std::shared_ptr<Metric> memory_total_;
        std::shared_ptr<Metric> memory_used_;
        std::shared_ptr<Metric> memory_free_;
        std::shared_ptr<Metric> memory_wired_;
        
        // CPU metrics
        std::shared_ptr<MetricFamily> cpu_usage_;
        std::shared_ptr<Metric> cpu_percent_;
        
        // Uptime metrics
        std::shared_ptr<MetricFamily> uptime_;
        std::shared_ptr<Metric> uptime_seconds_;
        
        // Disk metrics
        std::shared_ptr<MetricFamily> disk_bytes_;
        std::shared_ptr<Metric> disk_total_;
        std::shared_ptr<Metric> disk_used_;
        std::shared_ptr<Metric> disk_free_;
        
        // GPU metrics
        std::shared_ptr<MetricFamily> gpu_info_;
        std::shared_ptr<MetricFamily> gpu_usage_;
        std::shared_ptr<MetricFamily> gpu_memory_bytes_;
        std::shared_ptr<MetricFamily> gpu_temperature_;
        
        std::shared_ptr<Metric> gpu_info_metric_;
        std::shared_ptr<Metric> gpu_usage_percent_;
        std::shared_ptr<Metric> gpu_memory_total_;
        std::shared_ptr<Metric> gpu_memory_used_;
        std::shared_ptr<Metric> gpu_memory_free_;
        std::shared_ptr<Metric> gpu_temp_celsius_;
        
        // Collection methods
        void collect_memory_metrics();
        void collect_cpu_metrics();
        void collect_uptime_metrics();
        void collect_disk_metrics();
        void collect_gpu_metrics();
        
        // Platform-specific helpers
        struct MemoryInfo {
            uint64_t total_physical = 0;
            uint64_t used_physical = 0;
            uint64_t free_physical = 0;
            uint64_t wired = 0;
        };
        
        struct DiskInfo {
            uint64_t total = 0;
            uint64_t used = 0;
            uint64_t free = 0;
        };
        
        struct GpuInfo {
            int count = 0;
            double usage_percent = 0.0;
            uint64_t memory_total = 0;
            uint64_t memory_used = 0;
            uint64_t memory_free = 0;
            double temperature = 0.0;
            std::string type = "unknown";
            std::string name = "unknown";
        };
        
        MemoryInfo get_memory_info();
        double get_cpu_usage();
        uint64_t get_system_uptime();
        DiskInfo get_disk_info();
        GpuInfo get_gpu_info();
        bool has_gpu_support();
    };

    } // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_SYSTEM_METRICS_HPP