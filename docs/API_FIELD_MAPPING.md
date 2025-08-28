# API Field Name Mapping Documentation

This document provides a comprehensive mapping of all request and response field names used in the kolosal-server API to ensure consistency and backward compatibility.

## Table of Contents
1. [Authentication & Configuration](#authentication--configuration)
2. [Downloads Management](#downloads-management)
3. [Models Management](#models-management)
4. [Inference Engines](#inference-engines)
5. [Document Operations](#document-operations)
6. [Text Processing](#text-processing)
7. [Server Monitoring](#server-monitoring)
8. [Completion Endpoints](#completion-endpoints)

---

## Authentication & Configuration

### `/auth/config` - Rate Limit Clearing

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `client_ip` | string | IP address to clear rate limit for | No |
| `clear_all` | boolean | Clear all rate limits | No |

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `message` | string | Success message |
| `status` | string | Always "success" |

### `/auth/config` - Configuration Update

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `rate_limiter.max_requests` | integer | Maximum requests allowed | No |
| `rate_limiter.window_size` | integer | Time window in seconds | No |
| `cors.allowed_origins` | array | List of allowed origins | No |
| `cors.allowed_methods` | array | List of allowed HTTP methods | No |
| `api_key.api_keys` | array | List of valid API keys | No |

---

## Downloads Management

### `/downloads/{id}` - Download Progress

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `model_id` | string | Model identifier |
| `type` | string | Model type (llm/embedding) |
| `status` | string | Download status |
| `progress.downloaded_bytes` | integer | Bytes downloaded |
| `progress.total_bytes` | integer | Total file size |
| `progress.percentage` | float | Download percentage |
| `progress.download_speed_bps` | integer | Speed in bytes/sec |
| `timing.start_time` | integer | Start timestamp (ms) |
| `timing.elapsed_seconds` | float | Time elapsed |
| `error_message` | string | Error details if failed |

### `/downloads` - List All Downloads

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `active_downloads` | array | Array of download objects |
| `summary.total_active` | integer | Total active downloads |
| `summary.startup_downloads` | integer | Startup downloads count |
| `summary.regular_downloads` | integer | Regular downloads count |
| `summary.embedding_model_downloads` | integer | Embedding model downloads count |
| `summary.llm_model_downloads` | integer | LLM model downloads count |
| `timestamp` | integer | Response timestamp (ms) |

---

## Models Management

### `/models` - List Models

#### Standard Format Response
| Field Name | Type | Description |
|------------|------|-------------|
| `models` | array | Array of model objects |
| `total_count` | integer | Total number of models |
| `summary.total_models` | integer | Total models count |
| `summary.embedding_models` | integer | Embedding models count |
| `summary.llm_models` | integer | LLM models count |
| `summary.loaded_models` | integer | Loaded models count |
| `summary.unloaded_models` | integer | Unloaded models count |

#### OpenAI Format Response (when path contains `/v1/`)
| Field Name | Type | Description |
|------------|------|-------------|
| `object` | string | Always "list" |
| `data` | array | Array of model objects |

#### Model Object Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `model_id` | string | Model identifier |
| `status` | string | "loaded" or "unloaded" |
| `available` | boolean | Model availability |
| `last_accessed` | string | Last access time |
| `model_type` | string | "embedding" or "llm" |
| `capabilities` | array | List of capabilities |
| `inference_ready` | boolean | Ready for inference |

### `/models` - Add Model

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `model_id` | string | Model identifier | Yes |
| `model_path` | string | Path or URL to model | Yes |
| `model_type` | string | "llm" or "embedding" | Yes |
| `load_immediately` | boolean | Load on addition | No |
| `loading_parameters.n_ctx` | integer | Context size | No |
| `loading_parameters.n_batch` | integer | Batch size | No |
| `loading_parameters.n_gpu_layers` | integer | GPU layers | No |

---

## Inference Engines

### `/engines` - Add Engine

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `name` | string | Engine name | Yes |
| `library_path` | string | Path to library | Yes |
| `description` | string | Engine description | No |
| `load_on_startup` | boolean | Auto-load on start | No |

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `name` | string | Engine name |
| `library_path` | string | Library path |
| `description` | string | Description |
| `load_on_startup` | boolean | Auto-load setting |
| `is_loaded` | boolean | Current load status |

### `/engines/set_default` - Set Default Engine

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `engine_name` | string | Name of engine to set as default | Yes |

---

## Document Operations

### `/documents/add` - Add Documents

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `documents` | array | Array of document objects | Yes |
| `documents[].text` | string | Document text | Yes |
| `documents[].metadata` | object | Document metadata | No |

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `successful_count` | integer | Successfully added |
| `failed_count` | integer | Failed to add |
| `results` | array | Array of result objects |
| `results[].id` | string | Document ID |
| `results[].success` | boolean | Operation success |
| `results[].error` | string | Error message if failed |
| `collection_name` | string | Collection name (always "documents") |

### `/documents/remove` - Remove Documents

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `ids` | array | Document IDs to remove | Yes |

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `removed_count` | integer | Successfully removed |
| `failed_count` | integer | Failed to remove |
| `not_found_count` | integer | IDs not found |

---

## Text Processing

### `/chunking` - Text Chunking

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `text` | string | Text to chunk | Yes |
| `method` | string | "regular" or "semantic" | Yes |
| `chunk_size` | integer | Target chunk size | No |
| `overlap` | integer | Overlap between chunks | No |
| `model_name` | string | Model for semantic chunking | No* |

*Required if method is "semantic"

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `chunks` | array | Array of chunk objects |
| `chunks[].text` | string | Chunk text |
| `chunks[].index` | integer | Chunk index |
| `chunks[].token_count` | integer | Token count |
| `model_name` | string | Model used |
| `method` | string | Chunking method |
| `total_chunks` | integer | Total number of chunks |
| `usage.original_tokens` | integer | Original text token count |
| `usage.total_chunk_tokens` | integer | Total tokens in all chunks |
| `usage.processing_time_ms` | float | Processing time |

### `/parse/pdf` - Parse PDF

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `data` | string | Base64-encoded PDF | Yes |
| `method` | string | "fast", "ocr", or "visual" | No |
| `language` | string | Language code | No |
| `progress` | boolean | Show progress | No |

### `/parse/docx` - Parse DOCX

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `data` | string | Base64-encoded DOCX | Yes |

### `/parse/html` - Parse HTML

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `html` | string | HTML content | Yes |

---

## Server Monitoring

### `/server/logs` - Get Server Logs

#### Response Fields
| Field Name | Type | Description |
|------------|------|-------------|
| `logs` | array | Array of log entries |
| `logs[].level` | string | "ERROR", "WARNING", "INFO", "DEBUG" |
| `logs[].timestamp` | string | Log timestamp |
| `logs[].message` | string | Log message |
| `total_count` | integer | Total log entries |
| `retrieved_at` | string | Retrieval timestamp |

---

## Completion Endpoints

### `/inference/completions` - Text Completion

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `prompt` | string | Input prompt | Yes |
| `max_tokens` | integer | Maximum tokens to generate | No |
| `temperature` | float | Sampling temperature | No |
| `stream` | boolean | Enable streaming | No |
| `model` | string | Model to use | No |

#### Response Fields (Non-streaming)
| Field Name | Type | Description |
|------------|------|-------------|
| `text` | string | Generated text |
| `tokens` | array | Token information |
| `tps` | float | Tokens per second |
| `ttft` | float | Time to first token |

#### Response Fields (Streaming - SSE format)
Each chunk contains:
| Field Name | Type | Description |
|------------|------|-------------|
| `text` | string | Generated text chunk |
| `tokens` | array | Token information |
| `tps` | float | Current tokens/sec |
| `ttft` | float | Time to first token |

### `/inference/chat/completions` - Chat Completion

#### Request Fields
| Field Name | Type | Description | Required |
|------------|------|-------------|----------|
| `messages` | array | Chat messages | Yes |
| `messages[].role` | string | "system", "user", or "assistant" | Yes |
| `messages[].content` | string | Message content | Yes |
| `max_tokens` | integer | Maximum tokens | No |
| `temperature` | float | Sampling temperature | No |
| `stream` | boolean | Enable streaming | No |

---

## Error Response Format

All error responses follow this structure:

```json
{
  "error": {
    "message": "Human-readable error message",
    "type": "error_type",
    "param": "parameter_name",  // Optional
    "code": "error_code",        // Optional
    "details": "Additional details"  // Optional
  }
}
```

### Error Types
- `invalid_request_error` - Bad request (400)
- `authentication_error` - Unauthorized (401)
- `permission_error` - Forbidden (403)
- `not_found_error` - Not found (404)
- `conflict_error` - Conflict (409)
- `rate_limit_error` - Too many requests (429)
- `server_error` - Internal error (500)
- `service_unavailable` - Service down (503)

---

## Notes

1. All timestamps are in milliseconds since epoch unless otherwise specified
2. All byte sizes are in bytes (not KB/MB)
3. Speeds are in bytes per second
4. Boolean fields default to `false` if not specified
5. Arrays default to empty `[]` if not specified
6. Objects default to empty `{}` if not specified

## Version History

- **v1.0.0** - Initial field mapping documentation
- **v1.0.1** - Added backward compatibility fields for error responses