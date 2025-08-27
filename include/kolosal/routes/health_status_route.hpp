#ifndef KOLOSAL_HEALTH_STATUS_ROUTE_HPP
#define KOLOSAL_HEALTH_STATUS_ROUTE_HPP

#include "base_route.hpp"

namespace kolosal {

    class HealthStatusRoute : public BaseRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    
    protected:
        std::string getAllowedMethods() const override {
            return "GET, OPTIONS";
        }
    
    private:
        std::string current_method_;
    };

} // namespace kolosal

#endif // KOLOSAL_HEALTH_STATUS_ROUTE_HPP
