#pragma once

#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/framework/common/FixedVector.h"

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
		FixedVector256k body;
		std::string content_type = "text/plain"; // Default
		int status_code = 404;					 // Default: NotFound (intuituve - meaning it's not been handled)
	};

	using WebRequestHandler = std::function<void(const WebRequest&, WebResponse&)>;

	class WebServer
	{
	  public:
		WebServer();
		~WebServer();

		void start(uint16_t port, const std::string& web_root_folder, WebRequestHandler handler = nullptr);
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

#else // !defined(ROBOTICK_PLATFORM_DESKTOP)

#include <cstdint>
#include <functional>
#include <string>

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
		std::string content_type = "text/plain";
		int status_code = 404;
	};

	using WebRequestHandler = std::function<void(const WebRequest&, WebResponse&)>;

	class WebServer
	{
	  public:
		inline WebServer() {}
		inline ~WebServer() {}

		inline void start(uint16_t /*port*/, const std::string& /*web_root_folder*/, WebRequestHandler /*handler*/ = nullptr) {}
		inline void stop() {}
		inline bool is_running() const { return false; }
	};
} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
