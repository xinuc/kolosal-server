# Kolosal Server API Compatibility Test Suite

This test suite uses Catch2 to verify API compatibility and ensure that refactored code maintains the same request/response formats as the original implementation.

## Building Tests

```bash
# Configure with tests enabled
cmake -B build -DBUILD_TESTS=ON

# Build the tests
cmake --build build --target api_compatibility_tests

# Or build all tests
cmake --build build
```

## Running Tests

```bash
# Run all API compatibility tests
./build/tests/api_compatibility_tests

# Run specific test categories
./build/tests/api_compatibility_tests "[auth]"
./build/tests/api_compatibility_tests "[models]"
./build/tests/api_compatibility_tests "[downloads]"

# Run with detailed output
./build/tests/api_compatibility_tests -v

# List all available tests
./build/tests/api_compatibility_tests --list-tests
```

## Test Structure

```
tests/
├── api/                  # API compatibility tests
│   ├── test_auth_config.cpp    # Auth configuration endpoints
│   ├── test_models.cpp         # Models management endpoints
│   ├── test_downloads.cpp      # Downloads management
│   ├── test_completions.cpp    # Text/chat completion endpoints
│   ├── test_embeddings.cpp     # Embedding generation endpoints
│   ├── test_documents.cpp      # Document management endpoints
│   ├── test_server_config.cpp  # Server configuration endpoints
│   ├── test_engines.cpp        # Engine management endpoints
│   └── test_chunking.cpp       # Text chunking endpoints
├── helpers/              # Test helper classes
│   ├── api_client.hpp/cpp      # HTTP client for API testing
│   ├── response_validator.hpp/cpp # Response validation utilities
│   ├── test_server.hpp/cpp     # Test server management
│   └── test_utils.hpp/cpp      # General test utilities
├── fixtures/             # Test data and configurations
│   └── test_config.json        # Test server configuration
└── unit/                 # Unit tests for internal components
```

## Helper Classes

### ApiClient
HTTP client for making API requests to the test server:
```cpp
ApiClient client("localhost", 3001);
auto response = client.getAuthConfig();
REQUIRE(response.status_code == 200);
```

### ResponseValidator
Utilities for validating response formats:
```cpp
ResponseValidator::checkAuthConfig(response.body);
ResponseValidator::checkErrorResponse(response.body);
```

### TestServer
Manages starting/stopping test server instances:
```cpp
ScopedTestServer server(3001);  // Auto-starts and stops
```

### TestUtils
General utilities for test data generation:
```cpp
auto config = TestUtils::getTestConfig();
auto doc = TestUtils::generateTestDocument();
```

## Writing New Tests

1. Create a new test file in `tests/api/`
2. Include necessary headers:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "../helpers/api_client.hpp"
#include "../helpers/response_validator.hpp"
```

3. Write test cases using Catch2 macros:
```cpp
TEST_CASE("Endpoint description", "[tags]") {
    ScopedTestServer server(3001);
    ApiClient client("localhost", 3001);
    
    SECTION("Test scenario") {
        auto response = client.someEndpoint();
        REQUIRE(response.status_code == 200);
        CHECK(response.body["field"] == expected_value);
    }
}
```

## Test Coverage Goals

The test suite aims to verify:
- ✅ Exact request format compatibility
- ✅ Exact response format compatibility
- ✅ Field name consistency
- ✅ Error response formats
- ✅ OpenAI format compatibility
- ✅ Standard format responses
- ✅ Streaming (SSE) responses
- ✅ Configuration persistence

## CI/CD Integration

Add to your CI pipeline:
```yaml
- name: Build with tests
  run: cmake -B build -DBUILD_TESTS=ON && cmake --build build
  
- name: Run API tests
  run: ./build/tests/api_compatibility_tests --reporter junit --out test-results.xml
```