# API Compatibility Test Suite

This comprehensive test suite ensures complete backward compatibility after the controller refactoring. All tests focus on verifying that endpoints accept the same requests and return identical response formats as the old implementation.

## Test Organization

Tests are organized by priority and endpoint, with emphasis on:
- **Request format compatibility** - Same field names and structure
- **Response format compatibility** - Exact same response structure and field names
- **Error format consistency** - Same error response formats
- **Streaming behavior** - Identical SSE streaming format
- **Status codes** - Same HTTP status codes

---

## Priority 1: CRITICAL COMPATIBILITY TESTS

These tests MUST pass for the system to be considered compatible.

### 1.1 Auth Configuration Endpoints

#### Test: Clear Rate Limit by IP
```bash
# Request
curl -X POST http://localhost:8080/auth/config \
  -H "Content-Type: application/json" \
  -d '{
    "action": "clear_rate_limit",
    "client_ip": "192.168.1.100"  # MUST be client_ip, not ip
  }'

# Expected Response (200 OK)
{
  "status": "success",
  "message": "Rate limit data cleared for client: 192.168.1.100"
}

# Verify Fields
✓ Uses "client_ip" not "ip" or "ip_address"
✓ Returns "status" and "message" fields
```

#### Test: Clear All Rate Limits
```bash
# Request
curl -X POST http://localhost:8080/auth/config \
  -H "Content-Type: application/json" \
  -d '{
    "action": "clear_rate_limit",
    "clear_all": true
  }'

# Expected Response (200 OK)
{
  "status": "success",
  "message": "All rate limit data cleared"
}
```

#### Test: Get Auth Config
```bash
# Request
curl -X GET http://localhost:8080/auth/config

# Expected Response Structure
{
  "rate_limiter": {
    "enabled": true,
    "max_requests": 100,
    "window_size": 60
  },
  "cors": {
    "enabled": true,
    "allowed_origins": ["*"]
  },
  "api_key": {
    "enabled": false,
    "key": ""
  }
}
```

### 1.2 Downloads Endpoints

#### Test: List All Downloads
```bash
# Request
curl -X GET http://localhost:8080/downloads

# Expected Response Structure (200 OK)
{
  "active_downloads": [  # MUST be "active_downloads", not "downloads"
    {
      "download_id": "download-123",
      "model_id": "llama-2-7b",
      "status": "downloading",
      "type": "llm",  # Field name is "type"
      "download_type": "regular",  # Separate field
      "progress": {
        "downloaded_bytes": 1024000,  # NOT "downloaded_size"
        "total_bytes": 5120000,       # NOT "total_size"
        "percent_complete": 20.0,
        "download_speed": 1048576,
        "estimated_time_remaining": 4
      },
      "timing": {
        "start_time": 1234567890,
        "elapsed_time": 5,
        "last_update": 1234567895
      },
      "error_message": ""  # NOT "error"
    }
  ],
  "summary": {
    "total_active": 2,  # NOT "active"
    "startup_downloads": 1,
    "regular_downloads": 1,
    "embedding_model_downloads": 0,
    "llm_model_downloads": 2
  },
  "timestamp": 1234567890
}

# Field Verification Checklist
✓ Root array is "active_downloads" not "downloads"
✓ Each download has "type" and "download_type" fields
✓ Progress fields use "_bytes" not "_size"
✓ Error field is "error_message" not "error"
✓ Summary uses specific field names
```

#### Test: Get Single Download
```bash
# Request
curl -X GET http://localhost:8080/downloads/{download_id}

# Expected Response Structure (200 OK)
{
  "download_id": "download-123",
  "model_id": "llama-2-7b",
  "status": "downloading",
  "type": "llm",  # ONLY "type" field, NO "download_type" here
  "progress": {
    "downloaded_bytes": 1024000,
    "total_bytes": 5120000,
    "percent_complete": 20.0
  },
  "timing": {
    "start_time": 1234567890,
    "elapsed_time": 5
  },
  "error_message": ""
}

# Verify: Single download has NO "download_type" field
```

### 1.3 Models Endpoints

#### Test: List Models - Standard Format
```bash
# Request
curl -X GET http://localhost:8080/models

# Expected Response Structure (200 OK)
{
  "models": [  # MUST be "models" array
    {
      "model_id": "llama-2-7b",  # NOT "id" or "model"
      "status": "loaded",         # "loaded" or "unloaded"
      "available": true,
      "last_accessed": "recently",
      "model_type": "llm",        # "llm" or "embedding"
      "capabilities": ["text_generation", "chat"],
      "inference_ready": true
    }
  ],
  "total_count": 2,
  "summary": {
    "total_models": 2,
    "embedding_models": 0,
    "llm_models": 2,
    "loaded_models": 1,
    "unloaded_models": 1
  }
}

# Field Verification
✓ Uses "model_id" not "id"
✓ Has summary object with counts
✓ Root has "models" array
```

#### Test: List Models - OpenAI Format
```bash
# Request
curl -X GET http://localhost:8080/v1/models

# Expected Response Structure (200 OK)
{
  "object": "list",  # MUST have object: "list"
  "data": [          # MUST be "data" array, not "models"
    {
      "model_id": "llama-2-7b",
      "status": "loaded",
      "available": true,
      "last_accessed": "recently",
      "model_type": "llm",
      "capabilities": ["text_generation", "chat"],
      "inference_ready": true
    }
  ]
}

# Verify: NO summary object in OpenAI format
```

#### Test: Add Model with URL
```bash
# Request
curl -X POST http://localhost:8080/models \
  -H "Content-Type: application/json" \
  -d '{
    "model_id": "new-model",
    "model_path": "https://example.com/model.gguf",
    "model_type": "llm",
    "load_immediately": false
  }'

# Expected Response (202 Accepted for URL, 201 Created for local)
{
  "status": "accepted",
  "message": "Model download started",
  "download_id": "download-456"
}
```

### 1.4 Engines Endpoints

#### Test: List Engines
```bash
# Request
curl -X GET http://localhost:8080/engines

# Expected Response Structure
{
  "inference_engines": [  # MUST be "inference_engines"
    {
      "name": "llama_engine",
      "library_path": "/path/to/llama.so",
      "description": "LLaMA inference engine",
      "loaded": true,
      "available": true,
      "load_on_startup": true
    }
  ],
  "default_engine": "llama_engine",
  "total_count": 1
}
```

#### Test: Set Default Engine
```bash
# Request
curl -X PUT http://localhost:8080/engines \
  -H "Content-Type: application/json" \
  -d '{
    "engine_name": "llama_engine"  # MUST be "engine_name" not "engine"
  }'

# Expected Response (200 OK)
{
  "status": "success",
  "message": "Default engine set to: llama_engine"
}
```

---

## Priority 2: COMPLETION & CHAT ENDPOINTS

### 2.1 Text Completions

#### Test: Non-Streaming Completion
```bash
# Request
curl -X POST http://localhost:8080/inference/completions \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Once upon a time",
    "max_tokens": 50,
    "temperature": 0.7,
    "streaming": false
  }'

# Expected Response (200 OK)
{
  "text": "Once upon a time, there was a kingdom...",
  "tokens": 15,
  "tps": 45.2,
  "ttft": 0.234
}

# Verify: Simple format, no OpenAI wrapper
```

#### Test: Streaming Completion
```bash
# Request
curl -X POST http://localhost:8080/inference/completions \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Hello world",
    "streaming": true
  }'

# Expected Response Headers
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive

# Expected Response Body (SSE Format)
data: {"text": "Hello", "tokens": 1}

data: {"text": "Hello world", "tokens": 2}

data: {"text": "Hello world!", "tokens": 3}

data: {"done": true, "text": "Hello world!", "tokens": 3, "tps": 30.5}

# Verify SSE Format
✓ Each line starts with "data: "
✓ Double newline after each chunk
✓ Final chunk has "done": true
```

### 2.2 Chat Completions

#### Test: Non-Streaming Chat
```bash
# Request
curl -X POST http://localhost:8080/inference/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "user", "content": "Hello"}
    ],
    "max_tokens": 50
  }'

# Expected Response (200 OK)
{
  "text": "Hello! How can I help you today?",  # NOT "response" - uses same format as text completion
  "tokens": 8,
  "tps": 42.3,
  "ttft": 0.189,
  "prompt_tokens": 5,
  "completion_tokens": 8,
  "total_tokens": 13
}

# Verify: Uses "text" field like text completion, NOT "response"
```

### 2.3 OpenAI-Compatible Completions

#### Test: OpenAI Chat Completion
```bash
# Request
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "llama-2-7b",
    "messages": [
      {"role": "user", "content": "Hello"}
    ],
    "stream": false
  }'

# Expected Response (200 OK)
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1234567890,
  "model": "llama-2-7b",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "Hello! How can I help you?"
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 8,
    "total_tokens": 18
  }
}

# Verify OpenAI Fields
✓ Has "id" with chatcmpl- prefix
✓ Has "object": "chat.completion"
✓ Has "created" timestamp
✓ Has "finish_reason"
✓ Has "usage" object
```

#### Test: OpenAI Streaming Chat
```bash
# Request
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "llama-2-7b",
    "messages": [{"role": "user", "content": "Hi"}],
    "stream": true
  }'

# Expected SSE Response
data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"llama-2-7b","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":""}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"llama-2-7b","choices":[{"index":0,"delta":{"content":"!"},"finish_reason":""}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"llama-2-7b","choices":[{"index":0,"delta":{"content":""},"finish_reason":"stop"}]}

data: [DONE]

# Verify Streaming Format
✓ Each chunk has same id
✓ object is "chat.completion.chunk"
✓ Final message is "data: [DONE]"
```

---

## Priority 3: DOCUMENT & RETRIEVAL ENDPOINTS

### 3.1 Document Operations

#### Test: Add Documents
```bash
# Request
curl -X POST http://localhost:8080/documents/add \
  -H "Content-Type: application/json" \
  -d '{
    "documents": [
      {
        "id": "doc1",
        "text": "Sample document",
        "metadata": {"author": "test"}
      }
    ],
    "collection": "default"
  }'

# Expected Response (200 OK)
{
  "results": [  # MUST be "results" not "document_ids"
    {
      "id": "doc1",
      "status": "added",
      "message": "Document added successfully"
    }
  ],
  "summary": {
    "total": 1,
    "successful": 1,
    "failed": 0
  }
}
```

#### Test: Remove Documents
```bash
# Request
curl -X POST http://localhost:8080/documents/remove \
  -H "Content-Type: application/json" \
  -d '{
    "ids": ["doc1", "doc2"]
  }'

# Expected Response (200 OK)
{
  "removed_count": 1,
  "failed_count": 0,
  "not_found_count": 1,
  "details": {
    "removed": ["doc1"],
    "not_found": ["doc2"]
  }
}
```

### 3.2 Embeddings

#### Test: Generate Embeddings (OpenAI Format)
```bash
# Request
curl -X POST http://localhost:8080/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{
    "input": "Hello world",
    "model": "text-embedding-ada-002"
  }'

# Expected Response (200 OK)
{
  "object": "list",
  "data": [
    {
      "object": "embedding",
      "embedding": [0.123, -0.456, ...],  # Float array
      "index": 0
    }
  ],
  "model": "text-embedding-ada-002",
  "usage": {
    "prompt_tokens": 2,
    "total_tokens": 2
  }
}

# Verify Fields
✓ Has "object": "list"
✓ Has "data" array
✓ Each item has "object": "embedding"
✓ Usage has prompt_tokens and total_tokens
```

### 3.3 Chunking

#### Test: Text Chunking
```bash
# Request
curl -X POST http://localhost:8080/chunking \
  -H "Content-Type: application/json" \
  -d '{
    "text": "Long document text here...",
    "method": "regular",
    "chunk_size": 500,
    "chunk_overlap": 50
  }'

# Expected Response (200 OK)
{
  "chunks": [
    {
      "text": "First chunk...",
      "start": 0,
      "end": 500,
      "token_count": 125  # MUST be "token_count" not "tokens"
    }
  ],
  "metadata": {
    "total_chunks": 5,
    "method": "regular",
    "chunk_size": 500,
    "overlap": 50
  },
  "usage": {
    "processing_time": 0.234,
    "total_tokens": 625
  }
}

# Verify: Uses "token_count" not "tokens"
```

---

## Priority 4: ERROR RESPONSE FORMATS

### 4.1 Standard Error Format

#### Test: Bad Request Error
```bash
# Request with invalid JSON
curl -X POST http://localhost:8080/models \
  -H "Content-Type: application/json" \
  -d '{'

# Expected Response (400 Bad Request)
{
  "error": {
    "message": "Invalid JSON in request body",
    "type": "invalid_request_error",
    "param": "body",
    "code": null
  }
}
```

#### Test: Not Found Error
```bash
# Request
curl -X GET http://localhost:8080/models/nonexistent

# Expected Response (404 Not Found)
{
  "error": {
    "message": "Model 'nonexistent' not found",
    "type": "not_found",
    "param": "model_id",
    "code": null
  }
}
```

### 4.2 Enhanced Error with Details

#### Test: Parse Document Error
```bash
# Request
curl -X POST http://localhost:8080/parse/pdf \
  -H "Content-Type: application/json" \
  -d '{}'

# Expected Response (400 Bad Request)
{
  "error": {
    "message": "Missing or invalid 'data' field",
    "details": "Expected base64-encoded document data as string",
    "type": "invalid_request_error",
    "param": "data"
  }
}

# Verify: Has both "message" and "details" fields
```

### 4.3 Service Errors

#### Test: Database Unavailable
```bash
# Request (when database is down)
curl -X POST http://localhost:8080/documents/add \
  -H "Content-Type: application/json" \
  -d '{"documents": [{"id": "1", "text": "test"}]}'

# Expected Response (503 Service Unavailable)
{
  "error": {
    "message": "Database connection failed",
    "type": "service_unavailable",
    "code": "service_unavailable"
  }
}
```

---

## Priority 5: SPECIAL CASES

### 5.1 CORS Headers

#### Test: Preflight Request
```bash
# Request
curl -X OPTIONS http://localhost:8080/models \
  -H "Origin: http://example.com" \
  -H "Access-Control-Request-Method: GET"

# Expected Headers (204 No Content)
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-API-Key
Access-Control-Max-Age: 86400
```

### 5.2 Request IDs

#### Test: Request ID Generation
```bash
# Various endpoints should generate proper request IDs

# Chunking: "chunk-{counter}-{timestamp}"
# Document Add: "doc-{counter}-{timestamp}"
# Document Remove: "rem-{counter}-{timestamp}"
# Document Retrieve: "ret-{counter}-{timestamp}"

# These should appear in response headers or logs
```

---

## Test Execution Scripts

### Automated Compatibility Test Runner

```python
#!/usr/bin/env python3
"""
API Compatibility Test Runner
Tests that new implementation maintains exact compatibility with old.
"""

import requests
import json
import sys
from typing import Dict, Any, List, Tuple

BASE_URL = "http://localhost:8080"
API_KEY = ""  # Set if needed

class CompatibilityTester:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.results = []
    
    def test_endpoint(
        self,
        name: str,
        method: str,
        path: str,
        data: Dict[str, Any] = None,
        expected_fields: List[str] = None,
        expected_status: int = 200
    ) -> bool:
        """Test a single endpoint for compatibility"""
        url = f"{BASE_URL}{path}"
        headers = {
            "Content-Type": "application/json",
            "X-API-Key": API_KEY
        } if API_KEY else {"Content-Type": "application/json"}
        
        try:
            if method == "GET":
                resp = requests.get(url, headers=headers)
            elif method == "POST":
                resp = requests.post(url, json=data, headers=headers)
            elif method == "PUT":
                resp = requests.put(url, json=data, headers=headers)
            elif method == "DELETE":
                resp = requests.delete(url, headers=headers)
            else:
                raise ValueError(f"Unknown method: {method}")
            
            # Check status code
            if resp.status_code != expected_status:
                self.failed += 1
                print(f"❌ {name}: Wrong status {resp.status_code} (expected {expected_status})")
                return False
            
            # Check response fields if JSON
            if expected_fields and resp.headers.get('content-type', '').startswith('application/json'):
                response_json = resp.json()
                missing_fields = []
                
                for field_path in expected_fields:
                    if not self._check_field_exists(response_json, field_path):
                        missing_fields.append(field_path)
                
                if missing_fields:
                    self.failed += 1
                    print(f"❌ {name}: Missing fields: {missing_fields}")
                    print(f"   Response: {json.dumps(response_json, indent=2)[:200]}...")
                    return False
            
            self.passed += 1
            print(f"✅ {name}")
            return True
            
        except Exception as e:
            self.failed += 1
            print(f"❌ {name}: Exception: {e}")
            return False
    
    def _check_field_exists(self, obj: Any, path: str) -> bool:
        """Check if a field exists in nested object using dot notation"""
        parts = path.split('.')
        current = obj
        
        for part in parts:
            if isinstance(current, dict):
                if part not in current:
                    return False
                current = current[part]
            elif isinstance(current, list) and len(current) > 0:
                # Check first item in list
                current = current[0]
                if part not in current:
                    return False
                current = current[part]
            else:
                return False
        
        return True
    
    def run_all_tests(self):
        """Run all compatibility tests"""
        print("=" * 60)
        print("API COMPATIBILITY TEST SUITE")
        print("=" * 60)
        
        # Priority 1: Critical Compatibility
        print("\n### PRIORITY 1: CRITICAL COMPATIBILITY ###\n")
        
        # Auth Config
        self.test_endpoint(
            "Auth Config GET",
            "GET", "/auth/config",
            expected_fields=["rate_limiter", "cors", "api_key"]
        )
        
        self.test_endpoint(
            "Auth Clear Rate Limit",
            "POST", "/auth/config",
            data={"action": "clear_rate_limit", "client_ip": "192.168.1.1"},
            expected_fields=["status", "message"]
        )
        
        # Downloads
        self.test_endpoint(
            "Downloads List",
            "GET", "/downloads",
            expected_fields=[
                "active_downloads",
                "summary.total_active",
                "summary.startup_downloads",
                "summary.regular_downloads"
            ]
        )
        
        # Models
        self.test_endpoint(
            "Models List (Standard)",
            "GET", "/models",
            expected_fields=[
                "models",
                "total_count",
                "summary.total_models",
                "summary.loaded_models"
            ]
        )
        
        self.test_endpoint(
            "Models List (OpenAI)",
            "GET", "/v1/models",
            expected_fields=["object", "data"]
        )
        
        # Engines
        self.test_endpoint(
            "Engines List",
            "GET", "/engines",
            expected_fields=["inference_engines", "default_engine", "total_count"]
        )
        
        # Priority 2: Completion Endpoints
        print("\n### PRIORITY 2: COMPLETION ENDPOINTS ###\n")
        
        self.test_endpoint(
            "Text Completion",
            "POST", "/inference/completions",
            data={"prompt": "test", "max_tokens": 10},
            expected_fields=["text", "tokens", "tps", "ttft"]
        )
        
        self.test_endpoint(
            "Chat Completion",
            "POST", "/inference/chat/completions",
            data={"messages": [{"role": "user", "content": "test"}]},
            expected_fields=["text", "tokens", "tps", "ttft"]  # Uses "text" not "response"
        )
        
        self.test_endpoint(
            "OpenAI Chat Completion",
            "POST", "/v1/chat/completions",
            data={"model": "test", "messages": [{"role": "user", "content": "hi"}]},
            expected_fields=["id", "object", "created", "choices", "usage.prompt_tokens"]
        )
        
        # Priority 3: Document & Retrieval
        print("\n### PRIORITY 3: DOCUMENT & RETRIEVAL ###\n")
        
        self.test_endpoint(
            "Chunking",
            "POST", "/chunking",
            data={"text": "test document", "method": "regular"},
            expected_fields=["chunks", "metadata.total_chunks"]
        )
        
        # Priority 4: Error Formats
        print("\n### PRIORITY 4: ERROR FORMATS ###\n")
        
        self.test_endpoint(
            "Invalid Model Request",
            "GET", "/models/nonexistent",
            expected_fields=["error.message", "error.type"],
            expected_status=404
        )
        
        # Summary
        print("\n" + "=" * 60)
        print(f"RESULTS: {self.passed} passed, {self.failed} failed")
        print("=" * 60)
        
        return self.failed == 0

if __name__ == "__main__":
    tester = CompatibilityTester()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
```

### Field Comparison Script

```bash
#!/bin/bash
# compare_responses.sh - Compare old vs new response formats

OLD_URL="http://localhost:8081"  # Old server
NEW_URL="http://localhost:8080"  # New server

compare_endpoint() {
    local name="$1"
    local method="$2"
    local path="$3"
    local data="$4"
    
    echo "Comparing: $name"
    
    if [ "$method" = "POST" ]; then
        old_resp=$(curl -s -X POST "$OLD_URL$path" -H "Content-Type: application/json" -d "$data")
        new_resp=$(curl -s -X POST "$NEW_URL$path" -H "Content-Type: application/json" -d "$data")
    else
        old_resp=$(curl -s "$OLD_URL$path")
        new_resp=$(curl -s "$NEW_URL$path")
    fi
    
    # Compare field names
    old_fields=$(echo "$old_resp" | jq -r 'paths(scalars) as $p | $p | join(".")')
    new_fields=$(echo "$new_resp" | jq -r 'paths(scalars) as $p | $p | join(".")')
    
    diff_output=$(diff <(echo "$old_fields" | sort) <(echo "$new_fields" | sort))
    
    if [ -z "$diff_output" ]; then
        echo "  ✅ Field structure matches"
    else
        echo "  ❌ Field differences detected:"
        echo "$diff_output"
    fi
}

# Run comparisons
compare_endpoint "Models List" "GET" "/models" ""
compare_endpoint "Downloads List" "GET" "/downloads" ""
compare_endpoint "Auth Config" "GET" "/auth/config" ""
compare_endpoint "Text Completion" "POST" "/inference/completions" '{"prompt":"test"}'
```

---

## Test Execution Plan

### Phase 1: Setup
1. Start old server on port 8081
2. Start new server on port 8080
3. Load test data into both

### Phase 2: Compatibility Testing
1. Run automated compatibility test suite
2. Document any failures with full request/response
3. Compare field-by-field using comparison script

### Phase 3: Performance Testing
1. Measure response times for key endpoints
2. Compare with old implementation
3. Flag any regression >50%

### Phase 4: Edge Cases
1. Test with empty/null values
2. Test with large payloads
3. Test Unicode and special characters

### Phase 5: Stress Testing
1. Concurrent request handling
2. Memory leak detection
3. Long-running stability test

## Success Criteria

### MUST PASS (Blocking)
- ✅ All Priority 1 tests pass
- ✅ Response formats match exactly
- ✅ Field names are identical
- ✅ Status codes are the same
- ✅ Error formats unchanged

### SHOULD PASS (Important)
- ✅ 95%+ of Priority 2-3 tests pass
- ✅ Performance within 50% of old
- ✅ No memory leaks detected
- ✅ Concurrent operations stable

### NICE TO HAVE
- ✅ All edge cases handled
- ✅ Improved error messages
- ✅ Better performance than old

## Issue Reporting Template

If a test fails, document:

```markdown
### Failed Test: [Test Name]

**Endpoint:** `METHOD /path`

**Request:**
```json
{
  "field": "value"
}
```

**Expected Response (Old):**
```json
{
  "expected": "format"
}
```

**Actual Response (New):**
```json
{
  "actual": "format"
}
```

**Field Differences:**
- Missing: `field_name`
- Added: `new_field`
- Renamed: `old_name` → `new_name`

**Impact:** Breaking/Non-breaking
**Priority:** P1/P2/P3
```

---

## Quick Test Commands

```bash
# Test all critical endpoints quickly
./run_compatibility_tests.py

# Compare specific endpoint
./compare_responses.sh

# Full test suite
make test-compatibility

# Generate test report
./generate_test_report.sh > test_results.md
```