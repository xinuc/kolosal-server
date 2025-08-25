/**
 * @file llm_metrics_usage_example.cpp
 * @brief Example showing how to integrate LLM metrics into request processing
 * 
 * This example demonstrates the complete usage of LLM metrics throughout
 * the request lifecycle, following patterns from vLLM, Ray Serve, and TGI.
 */

#include "kolosal/metrics/llm_metrics_integration.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace kolosal::metrics;

// Simulate a completion request processing
void process_completion_request(const std::string& request_id, 
                               const std::string& prompt, 
                               size_t max_tokens = 100) {
    std::cout << "Processing request: " << request_id << std::endl;
    
    // Calculate prompt tokens (simplified)
    size_t prompt_tokens = prompt.length() / 4; // ~4 chars per token
    
    // Create request tracker - automatically records start
    RequestTracker tracker(request_id, prompt_tokens, max_tokens);
    
    // Simulate queueing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tracker.queued();
    
    // Simulate scheduling delay
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tracker.scheduled();
    
    // Update active request counts (simulate engine state)
    update_request_counts(1, 0, 0); // 1 running, 0 waiting
    
    // Simulate prefill phase
    BatchTracker batch_tracker(1); // batch size of 1
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    batch_tracker.record_prefill_phase(prompt_tokens);
    
    // Generate first token
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tracker.first_token_generated();
    
    // Generate remaining tokens
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(20, 40);
    
    size_t tokens_generated = std::min(max_tokens, size_t(50)); // Cap for demo
    
    for (size_t i = 1; i < tokens_generated; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
        tracker.token_generated();
    }
    
    // Record decode phase
    batch_tracker.record_decode_phase(tokens_generated - 1);
    
    // Finish request successfully
    tracker.finish(FinishReason::COMPLETED);
    
    // Update active request counts
    update_request_counts(0, 0, 0); // No active requests
    
    std::cout << "Completed request: " << request_id << std::endl;
}

// Simulate model loading
void simulate_model_loading() {
    std::cout << "Loading model..." << std::endl;
    
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Simulate loading time
    auto end = std::chrono::steady_clock::now();
    
    double load_time = std::chrono::duration<double>(end - start).count();
    record_model_loading("llama-7b-chat", load_time);
    
    // Simulate GPU memory usage after model loading
    update_gpu_memory(6ULL * 1024 * 1024 * 1024, 8ULL * 1024 * 1024 * 1024); // 6GB used, 8GB total
    
    std::cout << "Model loaded in " << load_time << " seconds" << std::endl;
}

// Simulate KV cache updates during inference
void simulate_kv_cache_updates() {
    std::cout << "Updating KV cache metrics..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> cache_dist(0.3, 0.9);
    
    for (int i = 0; i < 10; ++i) {
        double cache_usage = cache_dist(gen);
        update_gpu_kv_cache(cache_usage * 100.0); // Convert to percentage
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Simulate engine iterations
void simulate_engine_iterations() {
    std::cout << "Recording engine iterations..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> tokens_dist(50, 200);
    std::uniform_int_distribution<> duration_dist(10, 30);
    
    for (int i = 0; i < 5; ++i) {
        size_t tokens = tokens_dist(gen);
        double duration_ms = duration_dist(gen);
        
        record_engine_iteration(tokens, duration_ms);
        
        std::cout << "Iteration " << i+1 << ": " << tokens << " tokens in " 
                  << duration_ms << "ms" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "=== LLM Metrics Usage Example ===" << std::endl;
    
    // Initialize metrics (this happens automatically when first accessed)
    std::cout << "Initializing metrics..." << std::endl;
    auto& metrics = LLMMetrics::instance();
    
    // Simulate model loading
    simulate_model_loading();
    
    std::cout << "\n=== Processing Requests ===" << std::endl;
    
    // Process multiple requests concurrently (simulated)
    std::vector<std::thread> request_threads;
    
    // Create several requests
    std::vector<std::pair<std::string, std::string>> requests = {
        {"req_001", "What is the capital of France?"},
        {"req_002", "Explain quantum computing in simple terms."},
        {"req_003", "Write a short story about a robot."},
        {"req_004", "Calculate the fibonacci sequence up to n=10"},
        {"req_005", "What are the benefits of renewable energy?"}
    };
    
    // Process requests with some overlap
    for (const auto& [req_id, prompt] : requests) {
        request_threads.emplace_back([req_id, prompt]() {
            process_completion_request(req_id, prompt);
        });
        
        // Small delay between starting requests
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Start background tasks
    std::thread cache_thread(simulate_kv_cache_updates);
    std::thread engine_thread(simulate_engine_iterations);
    
    // Wait for all requests to complete
    for (auto& thread : request_threads) {
        thread.join();
    }
    
    cache_thread.join();
    engine_thread.join();
    
    std::cout << "\n=== Final Statistics ===" << std::endl;
    
    // Get final statistics
    auto stats = get_llm_statistics();
    std::cout << "Total Requests: " << stats.total_requests << std::endl;
    std::cout << "Active Requests: " << stats.active_requests << std::endl;
    std::cout << "Completed Requests: " << stats.completed_requests << std::endl;
    std::cout << "Failed Requests: " << stats.failed_requests << std::endl;
    std::cout << "Avg TTFT: " << stats.avg_time_to_first_token_ms << "ms" << std::endl;
    std::cout << "Avg TPS: " << stats.avg_tokens_per_second << " tokens/sec" << std::endl;
    std::cout << "P50 Latency: " << stats.p50_latency_ms << "ms" << std::endl;
    std::cout << "P95 Latency: " << stats.p95_latency_ms << "ms" << std::endl;
    std::cout << "P99 Latency: " << stats.p99_latency_ms << "ms" << std::endl;
    
    // Simulate model unloading
    record_model_unloading("llama-7b-chat");
    
    std::cout << "\nExample completed! Check /metrics endpoint for Prometheus output." << std::endl;
    
    return 0;
}

/*
Example usage in actual request handlers:

// In completion route handler:
void CompletionRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Parse request
        auto request = parse_completion_request(body);
        std::string request_id = generate_request_id();
        
        // Create request tracker
        RequestTracker tracker(request_id, request.prompt_tokens, request.max_tokens);
        
        // Add to queue
        tracker.queued();
        update_request_counts(get_running_count(), get_waiting_count() + 1);
        
        // When scheduled
        tracker.scheduled();
        update_request_counts(get_running_count() + 1, get_waiting_count() - 1);
        
        // During generation
        for (auto token : generate_tokens(request)) {
            if (is_first_token) {
                tracker.first_token_generated();
                is_first_token = false;
            } else {
                tracker.token_generated();
            }
            
            // Update cache metrics periodically
            if (token_count % 10 == 0) {
                update_gpu_kv_cache(get_cache_utilization());
            }
        }
        
        // Finish request
        tracker.finish(determine_finish_reason(response));
        
        send_response(sock, response);
        
    } catch (const std::exception& e) {
        // If we have a tracker in scope, it will automatically mark as error
        send_error_response(sock, e.what());
    }
}

// In engine/inference loop:
void InferenceEngine::run_iteration() {
    auto start = std::chrono::steady_clock::now();
    
    // Process current batch
    size_t total_tokens = process_batch();
    
    auto end = std::chrono::steady_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Record iteration metrics
    record_engine_iteration(total_tokens, duration_ms);
    
    // Update active request counts
    update_request_counts(running_requests.size(), waiting_requests.size(), swapped_requests.size());
    
    // Update cache usage
    update_gpu_kv_cache(get_kv_cache_usage());
}
*/