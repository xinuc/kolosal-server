#include "kolosal/metrics/metric_registry.hpp"
#include <algorithm>

namespace kolosal {
namespace metrics {

    std::shared_ptr<Metric> MetricFamily::add(const std::map<std::string, std::string>& labels) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::shared_ptr<Metric> metric;
        
        switch (type_) {
            case MetricType::COUNTER:
                metric = std::make_shared<Counter>(name_, help_);
                break;
            case MetricType::GAUGE:
                metric = std::make_shared<Gauge>(name_, help_);
                break;
            default:
                throw std::runtime_error("Unsupported metric type");
        }
        
        metrics_.emplace_back(metric, labels);
        return metric;
    }

    std::shared_ptr<MetricFamily> MetricRegistry::create_family(
        const std::string& name,
        const std::string& help,
        MetricType type
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if family already exists
        auto it = std::find_if(families_.begin(), families_.end(),
            [&name](const std::shared_ptr<MetricFamily>& family) {
                return family->name() == name;
            });
        
        if (it != families_.end()) {
            return *it;
        }
        
        auto family = std::make_shared<MetricFamily>(name, help, type);
        families_.push_back(family);
        return family;
    }

} // namespace metrics
} // namespace kolosal