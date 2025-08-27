#ifndef KOLOSAL_INTERNET_SEARCH_ROUTE_HPP
#define KOLOSAL_INTERNET_SEARCH_ROUTE_HPP

#include "../route_interface.hpp"
#include "../../server_config.hpp"

namespace kolosal {


    class InternetSearchRoute : public IRoute {
    private:
        SearchConfig config_;
        
    public:
        explicit InternetSearchRoute(const SearchConfig& config);
        ~InternetSearchRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_INTERNET_SEARCH_ROUTE_HPP
