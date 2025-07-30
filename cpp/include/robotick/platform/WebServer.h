#pragma once

#include "robotick/framework/common/FixedVector.h"

#include <cstdint>

namespace robotick
{
	using WebRequestBodyBuffer = FixedVector1k;

#if defined(ROBOTICK_PLATFORM_ESP32)
	using WebResponseBodyBuffer = FixedVector2k;
#else
	using WebResponseBodyBuffer = FixedVector256k; // big enough for a generous jpg image
#endif

	struct WebRequest
	{
		FixedString128 uri;
		FixedString8 method;
		WebRequestBodyBuffer body;
	};

	struct WebResponse
	{
		WebResponseBodyBuffer body;
		FixedString32 content_type = "text/plain"; // Default
		int status_code = 404;					   // Default: NotFound (intuituve - meaning it's not been handled)
	};

	using WebRequestHandler = std::function<void(const WebRequest&, WebResponse&)>;

	struct WebServerImpl;

	class WebServer
	{
	  public:
		WebServer();
		~WebServer();

		void start(const char* name, uint16_t port, const char* web_root_folder = nullptr, WebRequestHandler handler = nullptr);
		void stop();
		bool is_running() const;

		WebRequestHandler get_handler() const { return handler; };
		const char* get_server_name() const { return server_name.c_str(); };
		const char* get_document_root() const { return document_root.c_str(); };

	  private:
		WebServerImpl* impl = nullptr;

		bool running = false;
		WebRequestHandler handler = nullptr;

		FixedString32 server_name;
		FixedString512 document_root;
	};
} // namespace robotick
