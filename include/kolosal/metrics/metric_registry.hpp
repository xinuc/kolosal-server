#ifndef KOLOSAL_METRIC_REGISTRY_HPP
#define KOLOSAL_METRIC_REGISTRY_HPP

#include "metric_types.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace kolosal {
namespace metrics {

    class MetricFamily {
    public:
        MetricFamily(const std::string& name, const std::string& help, MetricType type)
            : name_(name), help_(help), type_(type) {}
        
        const std::string& name() const { return name_; }
        const std::string& help() const { return help_; }
        MetricType type() const { return type_; }
        
        std::shared_ptr<Metric> add(const std::map<std::string, std::string>& labels = {});
        const std::vector<LabeledMetric>& metrics() const { return metrics_; }
        
    private:
        std::string name_;
        std::string help_;
        MetricType type_;
        std::vector<LabeledMetric> metrics_;
        mutable std::mutex mutex_;
    };

    class MetricRegistry {
    public:
        static MetricRegistry& instance() {
            static MetricRegistry registry;
            return registry;
        }
        
        std::shared_ptr<MetricFamily> create_family(
            const std::string& name,
            const std::string& help,
            MetricType type
        );
        
        const std::vector<std::shared_ptr<MetricFamily>>& families() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return families_;
        }
        
        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            families_.clear();
        }
        
    private:
        MetricRegistry() = default;
        std::vector<std::shared_ptr<MetricFamily>> families_;
        mutable std::mutex mutex_;
    };

    // Builder pattern for creating metrics
    class MetricBuilder {
    public:
        MetricBuilder() = default;
        
        MetricBuilder& name(const std::string& name) {
            name_ = name;
            return *this;
        }
        
        MetricBuilder& help(const std::string& help) {
            help_ = help;
            return *this;
        }
        
        std::shared_ptr<MetricFamily> counter() {
            return MetricRegistry::instance().create_family(name_, help_, MetricType::COUNTER);
        }
        
        std::shared_ptr<MetricFamily> gauge() {
            return MetricRegistry::instance().create_family(name_, help_, MetricType::GAUGE);
        }
        
        std::shared_ptr<MetricFamily> histogram() {
            return MetricRegistry::instance().create_family(name_, help_, MetricType::HISTOGRAM);
        }
        
    private:
        std::string name_;
        std::string help_;
    };

    // Convenience function to create metrics
    inline MetricBuilder build() {
        return MetricBuilder();
    }

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_METRIC_REGISTRY_HPP