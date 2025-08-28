# Kolosal Server API Compatibility Tests

# Fetch Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Fetch cpp-httplib for HTTP client
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.14.3
)
set(HTTPLIB_REQUIRE_OPENSSL OFF)
set(HTTPLIB_COMPILE ON)
FetchContent_MakeAvailable(httplib)

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Create test helper library
add_library(kolosal_test_helpers STATIC
    helpers/api_client.cpp
    helpers/response_validator.cpp
)

target_include_directories(kolosal_test_helpers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(kolosal_test_helpers
    PUBLIC
        httplib::httplib
        nlohmann_json::nlohmann_json
        Catch2::Catch2
)

# API Compatibility Tests
add_executable(api_compatibility_tests
    api/test_simple.cpp
    api/test_auth.cpp
    api/test_health.cpp
    api/test_models.cpp
    api/test_downloads.cpp
    api/test_completions.cpp
    api/test_embeddings.cpp
    api/test_documents.cpp
    api/test_server_logs.cpp
    api/test_engines.cpp
    api/test_chunking.cpp
    api/test_internet_search.cpp
    api/test_parse_document.cpp
    api/test_main.cpp
)

target_link_libraries(api_compatibility_tests
    PRIVATE
        kolosal_test_helpers
        Catch2::Catch2WithMain
)

# Add test discovery
include(CTest)
include(Catch)
catch_discover_tests(api_compatibility_tests)

