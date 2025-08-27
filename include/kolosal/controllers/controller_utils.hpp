#ifndef KOLOSAL_CONTROLLERS_CONTROLLER_UTILS_HPP
#define KOLOSAL_CONTROLLERS_CONTROLLER_UTILS_HPP

#include <json.hpp>
#include <string>
#include "inference_interface.h"

namespace kolosal {
namespace controllers {
namespace utils {

/**
 * Utility functions shared across controllers
 */
class ControllerUtils {
public:
    /**
     * Apply response format configuration to parameters
     * Used by both ChatCompletionController and CompletionController
     */
    static void applyResponseFormat(ChatCompletionParameters& params, const nlohmann::json& request);
    static void applyResponseFormat(CompletionParameters& params, const nlohmann::json& request);
    
    /**
     * Estimate token count from text
     * @param text Input text
     * @return Estimated token count
     */
    static size_t estimateTokenCount(const std::string& text);
    
    /**
     * Parse common completion parameters
     */
    static void parseCommonParameters(CompletionParameters& params, const nlohmann::json& j);
    static void parseCommonParameters(ChatCompletionParameters& params, const nlohmann::json& j);
    
    /**
     * Validate model type
     * @param modelType The model type to validate
     * @return true if valid, false otherwise
     */
    static bool isValidModelType(const std::string& modelType);
    
    /**
     * Get list of supported model types
     */
    static std::vector<std::string> getSupportedModelTypes();
    
private:
    // Token estimation factor (average characters per token)
    static constexpr size_t CHARS_PER_TOKEN = 4;
    
    // Supported model types
    static const std::vector<std::string> SUPPORTED_MODEL_TYPES;
};

} // namespace utils
} // namespace controllers
} // namespace kolosal

#endif // KOLOSAL_CONTROLLERS_CONTROLLER_UTILS_HPP