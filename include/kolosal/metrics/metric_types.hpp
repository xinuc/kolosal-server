#ifndef KOLOSAL_METRIC_TYPES_HPP
#define KOLOSAL_METRIC_TYPES_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <mutex>

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
            std::lock_guard<std::mutex> lock(mutex_);
            if (value >= value_) value_ = value;  // Counters only increase
        }
        
        void increment(double delta = 1.0) {
            std::lock_guard<std::mutex> lock(mutex_);
            value_ += delta;
        }
        
        double get() const override { 
            std::lock_guard<std::mutex> lock(mutex_);
            return value_; 
        }
        std::string prometheus_type() const override { return "counter"; }
        
    private:
        double value_;
        mutable std::mutex mutex_;
    };

    class Gauge : public Metric {
    public:
        Gauge(const std::string& name, const std::string& help)
            : Metric(name, help, MetricType::GAUGE), value_(0.0) {}
        
        void set(double value) override { 
            std::lock_guard<std::mutex> lock(mutex_);
            value_ = value; 
        }
        double get() const override { 
            std::lock_guard<std::mutex> lock(mutex_);
            return value_; 
        }
        std::string prometheus_type() const override { return "gauge"; }
        
    private:
        double value_;
        mutable std::mutex mutex_;
    };

    class Histogram : public Metric {
    public:
        Histogram(const std::string& name, const std::string& help,
                 const std::vector<double>& buckets = default_buckets())
            : Metric(name, help, MetricType::HISTOGRAM),
              buckets_(buckets),
              bucket_counts_(buckets.size() + 1, 0),  // +1 for +Inf bucket
              sum_(0.0),
              count_(0) {
            // Ensure buckets are sorted
            std::sort(buckets_.begin(), buckets_.end());
        }
        
        void observe(double value) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Find the appropriate bucket
            size_t bucket_index = buckets_.size();  // Default to +Inf bucket
            for (size_t i = 0; i < buckets_.size(); ++i) {
                if (value <= buckets_[i]) {
                    bucket_index = i;
                    break;
                }
            }
            
            // Increment all buckets from this index onwards (cumulative)
            // This is correct Prometheus histogram behavior
            for (size_t i = bucket_index; i < bucket_counts_.size(); ++i) {
                bucket_counts_[i]++;
            }
            
            sum_ += value;
            count_++;
        }
        
        void set(double value) override { observe(value); }
        double get() const override { return count_; }
        std::string prometheus_type() const override { return "histogram"; }
        
        const std::vector<double>& buckets() const { return buckets_; }
        std::vector<size_t> bucket_counts() const { 
            std::lock_guard<std::mutex> lock(mutex_);
            return bucket_counts_; 
        }
        double sum() const { 
            std::lock_guard<std::mutex> lock(mutex_);
            return sum_; 
        }
        size_t count() const { 
            std::lock_guard<std::mutex> lock(mutex_);
            return count_; 
        }
        
        static std::vector<double> default_buckets() {
            // Default buckets for HTTP latency in seconds
            return {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
        }
        
    private:
        std::vector<double> buckets_;
        std::vector<size_t> bucket_counts_;
        double sum_;
        size_t count_;
        mutable std::mutex mutex_;
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