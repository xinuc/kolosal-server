#ifndef KOLOSAL_ENGINES_ROUTE_HPP
#define KOLOSAL_ENGINES_ROUTE_HPP

#include "base_route.hpp"

namespace kolosal
{

    /**
     * @brief Route handler for managing inference engine libraries
     * 
     * Handles GET requests to list available inference engines,
     * POST requests to add new inference engines to the configuration,
     * and PUT requests to set the default inference engine
     * 
     * Supported endpoints:
     * - GET /engines or /v1/engines - List available engines with default engine info
     * - POST /engines or /v1/engines - Add new engine
     * - PUT /engines or /v1/engines - Set default engine
     */
    class EnginesRoute : public BaseRoute
    {
    public:
        /**
         * @brief Check if this route matches the given request
         * @param method HTTP method (GET, POST, or PUT)
         * @param path Request path (should be /inference-engines or /v1/inference-engines)
         * @return True if this route should handle the request
         */
        bool match(const std::string &method, const std::string &path) override;

        /**
         * @brief Handle the inference engines request
         * @param sock Socket to send response to
         * @param body Request body (used for POST requests to add engines)
         */
        void handle(SocketType sock, const std::string &body) override;
        
    protected:
        /**
         * @brief Get allowed methods for this route
         * @return Comma-separated list of allowed HTTP methods
         */
        std::string getAllowedMethods() const override {
            return "GET, POST, PUT, OPTIONS";
        }

    private:
        // Store the matched method to determine operation in handle()
        mutable std::string matched_method_;
    };

} // namespace kolosal

#endif // KOLOSAL_ENGINES_ROUTE_HPP
