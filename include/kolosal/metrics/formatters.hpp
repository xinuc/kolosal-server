#ifndef KOLOSAL_METRIC_FORMATTERS_HPP
#define KOLOSAL_METRIC_FORMATTERS_HPP

#include "metric_registry.hpp"
#include <sstream>
#include <json.hpp>

namespace kolosal {
namespace metrics {

    class MetricFormatter {
    public:
        virtual ~MetricFormatter() = default;
        virtual std::string format(const MetricRegistry& registry) const = 0;
        virtual std::string content_type() const = 0;
    };

    class PrometheusFormatter : public MetricFormatter {
    public:
        std::string format(const MetricRegistry& registry) const override;
        std::string content_type() const override {
            return "text/plain; version=0.0.4; charset=utf-8";
        }
        
    private:
        void write_metric_family(std::ostream& out, const MetricFamily& family) const;
        void write_labels(std::ostream& out, const std::map<std::string, std::string>& labels) const;
        std::string escape_label_value(const std::string& value) const;
    };

    class JsonFormatter : public MetricFormatter {
    public:
        std::string format(const MetricRegistry& registry) const override;
        std::string content_type() const override {
            return "application/json";
        }
        
    private:
        nlohmann::json metric_family_to_json(const MetricFamily& family) const;
    };

} // namespace metrics
} // namespace kolosal

#endif // KOLOSAL_METRIC_FORMATTERS_HPP