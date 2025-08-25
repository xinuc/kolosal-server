#ifndef KOLOSAL_METRIC_TYPES_HPP
#define KOLOSAL_METRIC_TYPES_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>

namespace kolosal {
namespace metrics {

    enum class MetricType {
        COUNTER,
        GAUGE,
        HISTOGRAM,
        SUMMARY
    };

    struct MetricValue {
        double value;
        std::chrono::milliseconds timestamp;
        
        MetricValue(double v) : value(v), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())) {}
    };

    class Metric {
    public:
        Metric(const std::string& name, const std::string& help, MetricType type)
            : name_(name), help_(help), type_(type) {}
        
        virtual ~Metric() = default;
        
        const std::string& name() const { return name_; }
        const std::string& help() const { return help_; }
        MetricType type() const { return type_; }
        
        virtual void set(double value) = 0;
        virtual double get() const = 0;
        virtual std::string prometheus_type() const = 0;
        
    protected:
        std::string name_;
        std::string help_;
        MetricType type_;
    };

    class Counter : public Metric {
    public:
        Counter(const std::string& name, const std::string& help)
            : Metric(name, help, MetricType::COUNTER), value_(0.0) {}
        
        void set(double value) override {
            if (value >= value_) value_ = value;  // Counters only increase
        }
        
        void increment(double delta = 1.0) {
            value_ += delta;
        }
        
        double get() const override { return value_; }
        std::string prometheus_type() const override { return "counter"; }
        
    private:
        double value_;
    };

    class Gauge : public Metric {
    public:
        Gauge(const std::string& name, const std::string& help)
            : Metric(name, help, MetricType::GAUGE), value_(0.0) {}
        
        void set(double value) override { value_ = value; }
        double get() const override { return value_; }
        std::string prometheus_type() const override { return "gauge"; }
        
    private:
        double value_;
    };

    class LabeledMetric {
    public:
        LabeledMetric(std::shared_ptr<Metric> metric, const std::map<std::string, std::string>& labels)
            : metric_(metric), labels_(labels) {}
        
        std::shared_ptr<Metric> metric() const { return metric_; }
        const std::map<std::string, std::string>& labels() const { return labels_; }
        
    private:
        std::shared_ptr<Metric> metric_;
        std::map<std::string, std::string> labels_;
    };

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_METRIC_TYPES_HPP