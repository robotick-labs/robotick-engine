#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct mg_context;
struct mg_connection;

namespace robotick
{
	struct WebRequest
	{
		std::string_view uri;
		std::string_view method;
		std::string body;
	};

	struct WebResponse
	{
		std::string body;
		std::string content_type = "text/plain"; // Default
		int status_code = 200;					 // Default: OK
	};

	using WebRequestHandler = std::function<void(const WebRequest&, WebResponse&)>;

	class WebServer
	{
	  public:
		WebServer();
		~WebServer();

		void start(uint16_t port, WebRequestHandler handler);
		void stop();
		bool is_running() const;

	  private:
		static int callback(struct mg_connection* conn, void* user_data);

		mg_context* ctx;
		bool running;
		WebRequestHandler handler;
		std::string document_root;
	};
} // namespace robotick
