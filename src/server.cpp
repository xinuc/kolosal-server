#include "kolosal/server.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/metrics/http_metrics.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>
#include <json.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace kolosal
{

	// Helper: extract client IP from socket address
	static std::string extractClientIP(const struct sockaddr_storage &client_addr)
	{
		char clientIP[INET6_ADDRSTRLEN];
#ifdef _WIN32
		inet_ntop(client_addr.ss_family,
				  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
				  clientIP, sizeof(clientIP));
#else
		inet_ntop(client_addr.ss_family,
				  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
				  clientIP, sizeof(clientIP));
#endif
		return std::string(clientIP);
	}
	// Helper: parse HTTP headers from request with improved error handling
	static std::map<std::string, std::string> parseHeaders(const std::string &request)
	{
		std::map<std::string, std::string> headers;

		// Find the first \r\n (end of request line)
		size_t start = request.find("\r\n");
		if (start == std::string::npos)
		{
			// Try with just \n in case of Unix line endings
			start = request.find('\n');
			if (start == std::string::npos)
				return headers;
			start += 1;
		}
		else
		{
			start += 2;
		}

		// Find the end of headers
		size_t end = request.find("\r\n\r\n", start);
		if (end == std::string::npos)
		{
			// Try with just \n\n
			end = request.find("\n\n", start);
			if (end == std::string::npos)
				end = request.length(); // Use entire string if no clear end
		}

		std::string headerSection = request.substr(start, end - start);
		
		// Parse each header line
		std::istringstream headerStream(headerSection);
		std::string line;

		while (std::getline(headerStream, line) && !line.empty())
		{
			// Remove both \r and \n characters
			line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
			
			if (line.empty()) continue;

			size_t colonPos = line.find(':');
			if (colonPos != std::string::npos)
			{
				std::string name = line.substr(0, colonPos);
				std::string value = line.substr(colonPos + 1);

				// Trim whitespace from name and value
				name.erase(0, name.find_first_not_of(" \t"));
				name.erase(name.find_last_not_of(" \t") + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t") + 1);

				// Convert header name to lowercase for case-insensitive lookup
				std::transform(name.begin(), name.end(), name.begin(), ::tolower);
				headers[name] = value;
			}
		}

		return headers;
	}

	// Helper: parse the first line of the HTTP request
	static void parse_request_line(const std::string &requestLine,
								   std::string &method, std::string &path)
	{
		size_t first = requestLine.find(' ');
		if (first == std::string::npos)
			return;
		method = requestLine.substr(0, first);
		size_t second = requestLine.find(' ', first + 1);
		if (second == std::string::npos)
			return;
		path = requestLine.substr(first + 1, second - first - 1);
	}

	// Thread function to handle a client request
	void handle_client(SocketType client_sock,
					   const std::vector<std::unique_ptr<IRoute>> &routes,
					   const std::string &request,
					   const char *clientIP)
	{
		// Parse the HTTP request line
		size_t pos = request.find("\r\n");
		if (pos == std::string::npos)
		{
			ServerLogger::logWarning("Malformed request received from %s", clientIP);
			send_response(client_sock, 400, "{\"error\":\"Bad Request\"}");
#ifdef _WIN32
			closesocket(client_sock);
#else
			close(client_sock);
#endif
			return;
		}
		std::string requestLine = request.substr(0, pos);
		std::string method, path;
		parse_request_line(requestLine, method, path);

		ServerLogger::logInfo("[Thread %u] Processing %s request for %s from %s",
							  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP);

		// Extract body (if any)
		size_t headerEnd = request.find("\r\n\r\n");
		std::string body;
		if (headerEnd != std::string::npos)
			body = request.substr(headerEnd + 4);
		
		// Set request context for metrics tracking and start tracking
		// Skip metrics endpoint itself to avoid self-measurement noise
		bool skip_metrics = (path == "/metrics" || path == "/v1/metrics" || path == "/system-metrics");
		printf("DEBUG: Setting context for %s %s, skip_metrics=%d\n", method.c_str(), path.c_str(), skip_metrics);
		kolosal::http_internal::set_request_context(method, path, body.size(), !skip_metrics);
		if (!skip_metrics) {
			printf("DEBUG: Recording request start for %s %s\n", method.c_str(), path.c_str());
			kolosal::metrics::HTTPMetricsCollector::instance().record_request_start(method, path);
		}

		bool routeFound = false;
		int response_status = 200; // Default assume success
		size_t response_size = 0; // Will be estimated
		
		for (const auto &route : routes)
		{
			if (route->match(method, path))
			{
				routeFound = true;
				// Routes handle their own responses
				route->handle(client_sock, body);
				
				// Record metrics using the actual response status captured in send_response
				auto& ctx = kolosal::http_internal::g_request_context;
				printf("DEBUG: Metrics check: enabled=%d, method=%s, path=%s, response_status=%d\n", 
					ctx.metrics_enabled, method.c_str(), path.c_str(), ctx.response_status);
				fflush(stdout);
				if (ctx.metrics_enabled) {
					// Use the actual response status if captured, otherwise default to 200
					int actual_status = (ctx.response_status != -1) ? ctx.response_status : 200;
					size_t actual_response_size = (ctx.response_size > 0) ? ctx.response_size : 0;
					
					ServerLogger::logDebug("Recording HTTP metrics for %s %s status=%d", 
						method.c_str(), path.c_str(), actual_status);
					kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
						method, path, actual_status, body.size(), actual_response_size,
						ctx.start_time);
					ctx.metrics_enabled = false;
				}
				break;
			}
		}

		if (!routeFound)
		{
			ServerLogger::logWarning("No route found for %s %s", method.c_str(), path.c_str());
			send_response(client_sock, 404, "{\"error\":\"Not Found\"}");
			// Metrics will be recorded using the context set by send_response
			auto& ctx = kolosal::http_internal::g_request_context;
			if (ctx.metrics_enabled && ctx.response_status != -1) {
				ServerLogger::logDebug("Recording HTTP metrics for 404 %s %s", method.c_str(), path.c_str());
				kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
					method, path, ctx.response_status, body.size(), ctx.response_size,
					ctx.start_time);
				ctx.metrics_enabled = false;
			}
		}
		
		// Clear request context after handling
		kolosal::http_internal::clear_request_context();

#ifdef _WIN32
		closesocket(client_sock);
#else
		close(client_sock);
#endif
		ServerLogger::logInfo("[Thread %u] Completed request for %s",
							  std::this_thread::get_id(), path.c_str());
	}
	Server::Server(const std::string &port, const std::string &host) : port(port), host(host), running(false)
	{
#ifdef _WIN32
		listen_sock = INVALID_SOCKET;
#else
		listen_sock = -1;
#endif
		// Initialize authentication middleware with default settings
		authMiddleware_ = std::make_unique<auth::AuthMiddleware>();
	}

	Server::~Server()
	{
		stop();
#ifdef _WIN32
		if (listen_sock != INVALID_SOCKET)
			closesocket(listen_sock);
		WSACleanup();
#else
		if (listen_sock != -1)
			close(listen_sock);
#endif
	}

	bool Server::init()
	{
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			ServerLogger::logError("WSAStartup failed");
			return false;
		}
#endif

		struct addrinfo hints, *servinfo, *p;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		int rv;
		// Use the specified host instead of NULL to bind to specific interface
		const char *bind_host = (host == "0.0.0.0") ? NULL : host.c_str();
		if ((rv = getaddrinfo(bind_host, port.c_str(), &hints, &servinfo)) != 0)
		{
#ifdef _WIN32
			ServerLogger::logError("getaddrinfo: %s", gai_strerrorA(rv));
#else
			ServerLogger::logError("getaddrinfo: %s", gai_strerror(rv));
#endif
			return false;
		}

		for (p = servinfo; p != nullptr; p = p->ai_next)
		{
#ifdef _WIN32
			listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listen_sock == INVALID_SOCKET)
				continue;
#else
			listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listen_sock == -1)
				continue;
#endif

			int yes = 1;
			if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
						   reinterpret_cast<const char *>(&yes),
						   sizeof(yes)) == -1)
			{
#ifdef _WIN32
				closesocket(listen_sock);
#else
				close(listen_sock);
#endif
				continue;
			}

			if (bind(listen_sock, p->ai_addr, static_cast<int>(p->ai_addrlen)) == -1)
			{
#ifdef _WIN32
				closesocket(listen_sock);
#else
				close(listen_sock);
#endif
				continue;
			}
			break;
		}
		freeaddrinfo(servinfo);

#ifdef _WIN32
		if (listen_sock == INVALID_SOCKET)
		{
#else
		if (p == nullptr)
		{
#endif
			ServerLogger::logError("Failed to bind socket");
			return false;
		}

		if (listen(listen_sock, 10) == -1)
		{
			ServerLogger::logError("Listen failed");
			return false;
		}

		ServerLogger::logInfo("Server initialized and listening on %s:%s",
							  host.c_str(), port.c_str());
		return true;
	}

	void Server::addRoute(std::unique_ptr<IRoute> route)
	{
		routes.push_back(std::move(route));
	}

	void Server::run()
	{
		running = true;
		ServerLogger::logInfo("Server entering main loop with concurrent request handling");

		while (running)
		{
			struct sockaddr_storage client_addr;
#ifdef _WIN32
			int sin_size = sizeof(client_addr);
#else
			socklen_t sin_size = sizeof(client_addr);
#endif

			// Setup select for timeout to check running flag periodically
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(listen_sock, &readfds);

			struct timeval tv;
			tv.tv_sec = 1; // 1 second timeout
			tv.tv_usec = 0;

			int select_result = select(static_cast<int>(listen_sock) + 1, &readfds, NULL, NULL, &tv);

			if (select_result == -1)
			{
				ServerLogger::logError("Select failed");
				break;
			}

			if (select_result == 0)
			{
				// Timeout occurred, check if we should continue running
				continue;
			}

			if (!FD_ISSET(listen_sock, &readfds))
			{
				continue;
			}

			SocketType client_sock = accept(listen_sock,
											reinterpret_cast<struct sockaddr *>(&client_addr),
											&sin_size);
#ifdef _WIN32
			if (client_sock == INVALID_SOCKET)
			{
#else
			if (client_sock == -1)
			{
#endif
				ServerLogger::logError("Accept failed");
				continue;
			}

			// Get client information for logging
			char clientIP[INET6_ADDRSTRLEN];
#ifdef _WIN32
			inet_ntop(client_addr.ss_family,
					  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
					  clientIP, sizeof(clientIP));
#else
			inet_ntop(client_addr.ss_family,
					  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
					  clientIP, sizeof(clientIP));
#endif
			ServerLogger::logDebug("New client connection from %s", clientIP);
			// Spawn a thread to handle this client
			std::thread([this, client_sock, clientIP]()
						{
							ServerLogger::logDebug("[Thread %d] Processing request from %s",
												  std::this_thread::get_id(), clientIP);

							// Read the HTTP request with improved error handling
							const int bufferSize = 16384; // Increased buffer size for larger headers
							char buffer[bufferSize];
							std::string request;
							
							// Read data in chunks until we have the complete headers
							bool headersComplete = false;
							int totalBytesReceived = 0;
							
							// Set socket timeout to prevent hanging
							struct timeval timeout;
							timeout.tv_sec = 30;  // 30 second timeout
							timeout.tv_usec = 0;
							setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
							
							while (!headersComplete && totalBytesReceived < bufferSize - 1)
							{
								int bytesReceived = recv(client_sock, buffer + totalBytesReceived, 
														bufferSize - 1 - totalBytesReceived, 0);
								
								if (bytesReceived <= 0)
								{
									if (totalBytesReceived == 0)
									{
										ServerLogger::logError("[Thread %d] Failed to read HTTP headers from %s", 
															   std::this_thread::get_id(), clientIP);
									}
									else
									{
										ServerLogger::logWarning("[Thread %d] Incomplete HTTP request from %s, using partial data", 
																std::this_thread::get_id(), clientIP);
									}
									break;
								}
								
								totalBytesReceived += bytesReceived;
								buffer[totalBytesReceived] = '\0';
								
								// Check if we have complete headers (indicated by \r\n\r\n)
								if (strstr(buffer, "\r\n\r\n") != nullptr)
								{
									headersComplete = true;
								}
							}
							
							if (totalBytesReceived == 0)
							{
								ServerLogger::logError("[Thread %d] No data received from %s", 
													   std::this_thread::get_id(), clientIP);
#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}
							
							request = std::string(buffer, totalBytesReceived);

							// Parse the HTTP request line
							size_t endOfLine = request.find("\r\n");
							if (endOfLine == std::string::npos)
							{
								ServerLogger::logWarning("[Thread %d] Malformed request received", std::this_thread::get_id());
								send_response(client_sock, 400, "{\"error\":\"Bad Request\"}");
#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}

							std::string requestLine = request.substr(0, endOfLine);
							std::string method, path;
							parse_request_line(requestLine, method, path);

							// Parse headers for authentication middleware
							auto headers = parseHeaders(request);ServerLogger::logDebug("[Thread %d] Processing %s request for %s from %s",
												  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP); // Process authentication middleware
							ServerLogger::logDebug("[Thread %d] Calling auth middleware for %s %s from %s",
												  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP);

							auth::AuthMiddleware::RequestInfo authRequest(method, path, clientIP);
							authRequest.headers = headers;

							auto authResult = authMiddleware_->processRequest(authRequest);

							ServerLogger::logDebug("[Thread %d] Auth middleware result - Allowed: %s, Status: %d, Reason: %s",
												  std::this_thread::get_id(),
												  authResult.allowed ? "true" : "false",
												  authResult.statusCode,
												  authResult.reason.c_str());							// Add OpenAI-compatible response headers
							std::map<std::string, std::string> responseHeaders = {
								{"Content-Type", "application/json"},
								{"X-Content-Type-Options", "nosniff"},
								{"X-Frame-Options", "DENY"},
								{"X-XSS-Protection", "1; mode=block"},
								{"Referrer-Policy", "strict-origin-when-cross-origin"}
							};

							// Overwrite merge to ensure dynamic CORS headers replace defaults
							for (const auto &kv : authResult.headers) {
								responseHeaders[kv.first] = kv.second;
							}

							// Ensure Access-Control-Allow-Origin not containing repeated comma '*'
							auto acaoIt = responseHeaders.find("Access-Control-Allow-Origin");
							if (acaoIt != responseHeaders.end()) {
								if (acaoIt->second.find(",") != std::string::npos) {
									// Simplify: if any '*' present reduce to single '*', else take first token
									if (acaoIt->second.find("*") != std::string::npos) {
										acaoIt->second = "*";
									} else {
										auto commaPos = acaoIt->second.find(',');
										acaoIt->second = acaoIt->second.substr(0, commaPos);
										// trim
										while (!acaoIt->second.empty() && (acaoIt->second.front()==' '||acaoIt->second.front()=='\t')) acaoIt->second.erase(acaoIt->second.begin());
										while (!acaoIt->second.empty() && (acaoIt->second.back()==' '||acaoIt->second.back()=='\t')) acaoIt->second.pop_back();
									}
								}
							}

							// Set default headers for all subsequent responses on this thread
							kolosal::http_internal::set_default_response_headers(responseHeaders);

							// Check if request is blocked by authentication
							if (!authResult.allowed)
							{
								nlohmann::json jError = {
									{"error", {{"message", authResult.reason}, {"type", authResult.statusCode == 429 ? "rate_limit_exceeded" : "authentication_error"}, {"code", authResult.statusCode}}}};

								send_response(client_sock, authResult.statusCode, jError.dump(), responseHeaders);

								ServerLogger::logWarning("[Thread %d] Request blocked: %s",
														 std::this_thread::get_id(), authResult.reason.c_str());
								
								// Record metrics for auth failure (no body yet, so size is 0)
								bool skip_metrics = (path == "/metrics" || path == "/v1/metrics" || path == "/system-metrics");
								if (!skip_metrics) {
									auto start_time = std::chrono::steady_clock::now();
									kolosal::metrics::HTTPMetricsCollector::instance().record_request_start(method, path);
									kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
										method, path, authResult.statusCode, 0, jError.dump().size(), start_time);
								}

#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								kolosal::http_internal::clear_default_response_headers();
								return;
							}

							// Handle CORS preflight requests
							if (authResult.isPreflight)
							{
								send_response(client_sock, authResult.statusCode, "", responseHeaders);
								ServerLogger::logDebug("[Thread %d] CORS preflight request handled",
													  std::this_thread::get_id());
								
								// Record metrics for preflight (no body yet, so size is 0)
								bool skip_metrics = (path == "/metrics" || path == "/v1/metrics" || path == "/system-metrics");
								if (!skip_metrics) {
									auto start_time = std::chrono::steady_clock::now();
									kolosal::metrics::HTTPMetricsCollector::instance().record_request_start(method, path);
									kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
										method, path, authResult.statusCode, 0, 0, start_time);
								}

#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								kolosal::http_internal::clear_default_response_headers();
								return;
							}							// Find Content-Length header (case-insensitive)
							int contentLength = 0;
							auto it = headers.find("content-length");
							if (it != headers.end())
							{
								try
								{
									contentLength = std::stoi(it->second);
									ServerLogger::logDebug("[Thread %d] Content-Length: %d",
														   std::this_thread::get_id(), contentLength);
								}
								catch (const std::exception &)
								{
									ServerLogger::logWarning("[Thread %d] Invalid Content-Length header: %s",
															 std::this_thread::get_id(), it->second.c_str());
								}
							}

							// Find the start of the body
							size_t bodyStart = request.find("\r\n\r\n");
							std::string body;

							if (bodyStart != std::string::npos)
							{
								// Extract the body we've already read
								body = request.substr(bodyStart + 4);

								// If Content-Length indicates there's more data to read
								if (contentLength > 0 && body.length() < static_cast<size_t>(contentLength))
								{
									int remaining = static_cast<int>(contentLength - body.length());
									std::vector<char> bodyBuffer(remaining + 1, 0);

									int totalRead = 0;
									while (totalRead < remaining)
									{
										int bytesRead = recv(client_sock, bodyBuffer.data() + totalRead,
															 remaining - totalRead, 0);
										if (bytesRead <= 0)
										{
											break; // Error or connection closed
										}
										totalRead += bytesRead;
									}

									if (totalRead > 0)
									{
										body.append(bodyBuffer.data(), totalRead);
									}

									ServerLogger::logDebug("[Thread %d] Read %d additional bytes for body",
														   std::this_thread::get_id(), totalRead);
								}
							} 
							
							// Set request context for metrics tracking (must be done in worker thread after body is read!)
							// Skip metrics endpoint itself to avoid self-measurement noise
							bool skip_metrics = (path == "/metrics" || path == "/v1/metrics" || path == "/system-metrics");
							kolosal::http_internal::set_request_context(method, path, body.size(), !skip_metrics);
							if (!skip_metrics) {
								kolosal::metrics::HTTPMetricsCollector::instance().record_request_start(method, path);
							}
							
							// Route the request
							bool routeFound = false;
							for (auto &route : routes)
							{
								if (route->match(method, path))
								{
									routeFound = true;
									try
									{
										// Note: Routes will need to be updated to handle authentication headers
										// For now, they'll work as before but won't include auth headers
										route->handle(client_sock, body);
									}
									catch (const std::exception &ex)
									{
										ServerLogger::logError("[Thread %d] Error in route handler: %s",
															   std::this_thread::get_id(), ex.what());

										// If we haven't sent a response yet, send an error
										nlohmann::json jError = {{"error", {{"message", std::string("Internal error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
										send_response(client_sock, 500, jError.dump(), responseHeaders);
									}
									
									// Record metrics after route completes
									auto& ctx = kolosal::http_internal::g_request_context;
									if (ctx.metrics_enabled && ctx.response_status != -1) {
										kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
											method, path, ctx.response_status, body.size(), ctx.response_size,
											ctx.start_time);
										ctx.metrics_enabled = false;
									}
									break;
								}
							}

							if (!routeFound)
							{
								ServerLogger::logWarning("[Thread %d] No route found for %s %s",
														 std::this_thread::get_id(), method.c_str(), path.c_str());

								nlohmann::json jError = {{"error", {{"message", "Not found"}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
								send_response(client_sock, 404, jError.dump(), responseHeaders);
								
								// Record metrics for 404 response
								auto& ctx = kolosal::http_internal::g_request_context;
								if (ctx.metrics_enabled && ctx.response_status != -1) {
									kolosal::metrics::HTTPMetricsCollector::instance().record_request_complete(
										method, path, ctx.response_status, body.size(), ctx.response_size,
										ctx.start_time);
									ctx.metrics_enabled = false;
								}
							}							
							
							ServerLogger::logDebug("[Thread %d] Completed request for %s",
							std::this_thread::get_id(), path.c_str());

							// Clear request context after handling
							kolosal::http_internal::clear_request_context();
							
#ifdef _WIN32
							closesocket(client_sock);
#else
							close(client_sock);
#endif
							kolosal::http_internal::clear_default_response_headers();
						})
				.detach(); // Detach the thread to handle the request independently
		}

		ServerLogger::logInfo("Server main loop exited");
	}

	void Server::stop()
	{
		if (running)
		{
			ServerLogger::logInfo("Stopping server");
			running = false;

			// Additional cleanup could be added here
		}
	}

} // namespace kolosal