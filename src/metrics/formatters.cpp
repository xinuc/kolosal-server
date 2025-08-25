#include "kolosal/metrics/formatters.hpp"
#include <iomanip>
#include <algorithm>

namespace kolosal {
namespace metrics {

    std::string PrometheusFormatter::format(const MetricRegistry& registry) const {
        std::ostringstream out;
        
        for (const auto& family : registry.families()) {
            write_metric_family(out, *family);
            out << "\n";
        }
        
        return out.str();
    }

    void PrometheusFormatter::write_metric_family(std::ostream& out, const MetricFamily& family) const {
        if (family.metrics().empty()) {
            return;
        }
        
        // Write HELP comment
        out << "# HELP " << family.name() << " " << family.help() << "\n";
        
        // Write TYPE comment
        out << "# TYPE " << family.name() << " ";
        switch (family.type()) {
            case MetricType::COUNTER:
                out << "counter";
                break;
            case MetricType::GAUGE:
                out << "gauge";
                break;
            case MetricType::HISTOGRAM:
                out << "histogram";
                break;
            case MetricType::SUMMARY:
                out << "summary";
                break;
        }
        out << "\n";
        
        // Write metrics
        for (const auto& labeled_metric : family.metrics()) {
            out << family.name();
            
            // Write labels if they exist
            const auto& labels = labeled_metric.labels();
            if (!labels.empty()) {
                write_labels(out, labels);
            }
            
            // Write value
            out << " " << labeled_metric.metric()->get() << "\n";
        }
    }

    void PrometheusFormatter::write_labels(std::ostream& out, const std::map<std::string, std::string>& labels) const {
        if (labels.empty()) {
            return;
        }
        
        out << "{";
        bool first = true;
        for (const auto& [key, value] : labels) {
            if (!first) {
                out << ",";
            }
            out << key << "=\"" << escape_label_value(value) << "\"";
            first = false;
        }
        out << "}";
    }

    std::string PrometheusFormatter::escape_label_value(const std::string& value) const {
        std::string escaped;
        escaped.reserve(value.size() * 2);
        
        for (char c : value) {
            switch (c) {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                default:
                    escaped += c;
                    break;
            }
        }
        
        return escaped;
    }

    std::string JsonFormatter::format(const MetricRegistry& registry) const {
        nlohmann::json root;
        root["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        nlohmann::json metrics = nlohmann::json::array();
        
        for (const auto& family : registry.families()) {
            metrics.push_back(metric_family_to_json(*family));
        }
        
        root["metrics"] = metrics;
        return root.dump(2);
    }

    nlohmann::json JsonFormatter::metric_family_to_json(const MetricFamily& family) const {
        nlohmann::json family_json;
        family_json["name"] = family.name();
        family_json["help"] = family.help();
        family_json["type"] = [&family]() {
            switch (family.type()) {
                case MetricType::COUNTER: return "counter";
                case MetricType::GAUGE: return "gauge";
                case MetricType::HISTOGRAM: return "histogram";
                case MetricType::SUMMARY: return "summary";
                default: return "unknown";
            }
        }();
        
        nlohmann::json samples = nlohmann::json::array();
        
        for (const auto& labeled_metric : family.metrics()) {
            nlohmann::json sample;
            sample["labels"] = labeled_metric.labels();
            sample["value"] = labeled_metric.metric()->get();
            samples.push_back(sample);
        }
        
        family_json["samples"] = samples;
        return family_json;
    }

} // namespace metrics
} // namespace kolosal