#ifndef KOLOSAL_CONTROLLERS_SERVER_LOGS_CONTROLLER_HPP
#define KOLOSAL_CONTROLLERS_SERVER_LOGS_CONTROLLER_HPP

#include "kolosal/controllers/base_controller.hpp"

namespace kolosal {
namespace controllers {

class ServerLogsController : public BaseController {
public:
    ServerLogsController() = default;
    ~ServerLogsController() = default;

    // Get all server logs
    Response getLogs();
    
    // Get logs with filtering options
    Response getLogs(int limit, const std::string& level = "");

private:
    nlohmann::json formatLogEntry(const std::string& level, 
                                   const std::string& timestamp, 
                                   const std::string& message);
};

} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_SERVER_LOGS_CONTROLLER_HPP