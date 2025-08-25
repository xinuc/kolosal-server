#include "kolosal/metrics/system_metrics.hpp"
#include "kolosal/gpu_detection.hpp"
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_host.h>
#else
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <filesystem>
#include <regex>
#endif

namespace kolosal
{
    namespace metrics
    {

    SystemMetricsCollector::SystemMetricsCollector() {
        register_metrics();
    }

    void SystemMetricsCollector::register_metrics() {
        auto& registry = MetricRegistry::instance();
        
        // Server info
        server_info_ = build()
            .name("kolosal_server_info")
            .help("Server information")
            .gauge();
        server_info_->add({{"version", "1.0.0"}, {"name", "Kolosal_Inference_Server"}});
        
        // Memory metrics
        memory_bytes_ = build()
            .name("kolosal_memory_usage_bytes")
            .help("Memory usage in bytes")
            .gauge();
        
        memory_total_ = memory_bytes_->add({{"type", "total_physical"}});
        memory_used_ = memory_bytes_->add({{"type", "used_physical"}});
        memory_free_ = memory_bytes_->add({{"type", "free_physical"}});
        memory_wired_ = memory_bytes_->add({{"type", "wired"}});
        
        // CPU metrics
        cpu_usage_ = build()
            .name("kolosal_cpu_usage_percent")
            .help("CPU usage percentage")
            .gauge();
        
        cpu_percent_ = cpu_usage_->add();
        
        // Uptime metrics
        uptime_ = build()
            .name("kolosal_uptime_seconds")
            .help("System uptime in seconds")
            .counter();
        
        uptime_seconds_ = uptime_->add();
        
        // Disk metrics
        disk_bytes_ = build()
            .name("kolosal_disk_usage_bytes")
            .help("Disk usage in bytes")
            .gauge();
        
        disk_total_ = disk_bytes_->add({{"type", "total"}});
        disk_used_ = disk_bytes_->add({{"type", "used"}});
        disk_free_ = disk_bytes_->add({{"type", "free"}});
        
        // GPU metrics (conditionally registered)
        if (has_gpu_support()) {
            gpu_info_ = build()
                .name("kolosal_gpu_info")
                .help("GPU information")
                .gauge();
            
            gpu_usage_ = build()
                .name("kolosal_gpu_usage_percent")
                .help("GPU usage percentage")
                .gauge();
            
            gpu_memory_bytes_ = build()
                .name("kolosal_gpu_memory_bytes")
                .help("GPU memory usage in bytes")
                .gauge();
            
            gpu_temperature_ = build()
                .name("kolosal_gpu_temperature_celsius")
                .help("GPU temperature in Celsius")
                .gauge();
        }
    }

    void SystemMetricsCollector::collect() {
        collect_memory_metrics();
        collect_cpu_metrics();
        collect_uptime_metrics();
        collect_disk_metrics();
        
        if (has_gpu_support()) {
            collect_gpu_metrics();
        }
    }

    void SystemMetricsCollector::collect_memory_metrics() {
        auto mem_info = get_memory_info();
        
        memory_total_->set(mem_info.total_physical);
        memory_used_->set(mem_info.used_physical);
        memory_free_->set(mem_info.free_physical);
        memory_wired_->set(mem_info.wired);
    }

    void SystemMetricsCollector::collect_cpu_metrics() {
        cpu_percent_->set(get_cpu_usage());
    }

    void SystemMetricsCollector::collect_uptime_metrics() {
        uptime_seconds_->set(get_system_uptime());
    }

    void SystemMetricsCollector::collect_disk_metrics() {
        auto disk_info = get_disk_info();
        
        disk_total_->set(disk_info.total);
        disk_used_->set(disk_info.used);
        disk_free_->set(disk_info.free);
    }

    void SystemMetricsCollector::collect_gpu_metrics() {
        auto gpu_info = get_gpu_info();
        
        // Register GPU info metric with labels if not already done
        if (!gpu_info_metric_) {
            gpu_info_metric_ = gpu_info_->add({
                {"type", gpu_info.type}, 
                {"name", gpu_info.name}
            });
            
            gpu_usage_percent_ = gpu_usage_->add();
            gpu_memory_total_ = gpu_memory_bytes_->add({{"type", "total"}});
            gpu_memory_used_ = gpu_memory_bytes_->add({{"type", "used"}});
            gpu_memory_free_ = gpu_memory_bytes_->add({{"type", "free"}});
            gpu_temp_celsius_ = gpu_temperature_->add();
        }
        
        gpu_info_metric_->set(gpu_info.count);
        gpu_usage_percent_->set(gpu_info.usage_percent);
        gpu_memory_total_->set(gpu_info.memory_total);
        gpu_memory_used_->set(gpu_info.memory_used);
        gpu_memory_free_->set(gpu_info.memory_free);
        gpu_temp_celsius_->set(gpu_info.temperature);
    }

    SystemMetricsCollector::MemoryInfo SystemMetricsCollector::get_memory_info() {
        MemoryInfo info;
        
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            info.total_physical = memInfo.ullTotalPhys;
            info.used_physical = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
            info.free_physical = memInfo.ullAvailPhys;
        }
#elif defined(__APPLE__)
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        size_t length = sizeof(info.total_physical);
        sysctl(mib, 2, &info.total_physical, &length, NULL, 0);
        
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(natural_t);
        
        host_page_size(mach_host_self(), &page_size);
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
            info.free_physical = vm_stat.free_count * page_size;
            info.used_physical = info.total_physical - info.free_physical;
            info.wired = vm_stat.wire_count * page_size;
        }
#else
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            info.total_physical = memInfo.totalram * memInfo.mem_unit;
            info.used_physical = (memInfo.totalram - memInfo.freeram) * memInfo.mem_unit;
            info.free_physical = memInfo.freeram * memInfo.mem_unit;
        }
#endif
        
        return info;
    }

    double SystemMetricsCollector::get_cpu_usage() {
#ifdef _WIN32
        return 0.0; // Placeholder
#elif defined(__APPLE__)
        return 0.0; // Placeholder - would need sampling over time
#else
        std::ifstream file("/proc/stat");
        if (!file.is_open()) {
            return 0.0;
        }
        
        std::string line;
        std::getline(file, line);
        
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle, iowait = 0, irq = 0, softirq = 0;
        
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        
        long totalIdle = idle + iowait;
        long totalNonIdle = user + nice + system + irq + softirq;
        long total = totalIdle + totalNonIdle;
        
        return total > 0 ? ((double)totalNonIdle / total) * 100.0 : 0.0;
#endif
    }

    uint64_t SystemMetricsCollector::get_system_uptime() {
#ifdef _WIN32
        return GetTickCount64() / 1000;
#elif defined(__APPLE__)
        struct timeval boottime;
        size_t len = sizeof(boottime);
        int mib[2] = {CTL_KERN, KERN_BOOTTIME};
        
        if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0) {
            time_t now;
            time(&now);
            return now - boottime.tv_sec;
        }
        return 0;
#else
        struct sysinfo info;
        return (sysinfo(&info) == 0) ? info.uptime : 0;
#endif
    }

    SystemMetricsCollector::DiskInfo SystemMetricsCollector::get_disk_info() {
        DiskInfo info;
        
#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceEx(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
            info.total = totalBytes.QuadPart;
            info.free = totalFreeBytes.QuadPart;
            info.used = info.total - info.free;
        }
#else
        struct statvfs stat;
        if (statvfs("/", &stat) == 0) {
            info.total = stat.f_blocks * stat.f_frsize;
            info.free = stat.f_bavail * stat.f_frsize;
            info.used = info.total - info.free;
        }
#endif
        
        return info;
    }

    SystemMetricsCollector::GpuInfo SystemMetricsCollector::get_gpu_info() {
        GpuInfo info;
        
#ifdef __APPLE__
        info.count = 1;
        info.type = "metal";
        info.name = "Apple_GPU";
        
        size_t len = 0;
        if (sysctlbyname("hw.model", nullptr, &len, nullptr, 0) == 0) {
            char* model = new char[len];
            if (sysctlbyname("hw.model", model, &len, nullptr, 0) == 0) {
                std::string modelName(model);
                if (modelName.find("Pro") != std::string::npos) {
                    info.name = "Apple_Pro";
                }
            }
            delete[] model;
        }
        
        // Estimate GPU memory as 75% of system memory on Apple Silicon
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t totalPhysMem;
        size_t length = sizeof(totalPhysMem);
        if (sysctl(mib, 2, &totalPhysMem, &length, NULL, 0) == 0) {
            info.memory_total = static_cast<uint64_t>(totalPhysMem * 0.75);
            info.memory_free = info.memory_total; // Assume all free for now
        }
#else
        // Linux GPU detection
        if (std::filesystem::exists("/usr/bin/nvidia-smi")) {
            info.type = "nvidia";
            // Would need to call nvidia-smi for real data
        }
#endif
        
        return info;
    }

    bool SystemMetricsCollector::has_gpu_support() {
#ifdef __APPLE__
        return true; // Metal is available on modern macOS
#else
        return hasVulkanCapableGPU();
#endif
    }

    } // namespace metrics
} // namespace kolosal