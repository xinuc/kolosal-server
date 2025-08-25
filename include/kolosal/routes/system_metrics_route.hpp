#ifndef KOLOSAL_SYSTEM_METRICS_ROUTE_HPP
#define KOLOSAL_SYSTEM_METRICS_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal {

    class SystemMetricsRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    
    private:
        std::string current_method_;
        
        // Helper method to add engine metrics to registry
        void add_engine_metrics();
    };

} // namespace kolosal

#endif // KOLOSAL_SYSTEM_METRICS_ROUTE_HPP