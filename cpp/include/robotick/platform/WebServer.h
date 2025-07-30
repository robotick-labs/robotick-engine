#pragma once

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
		FixedString128 uri;
		FixedString8 method;
		FixedVector256k body;
	};

	struct WebResponse
	{
		FixedVector256k body;
		FixedString32 content_type = "text/plain"; // Default
		int status_code = 404;					   // Default: NotFound (intuituve - meaning it's not been handled)
	};

	using WebRequestHandler = std::function<void(const WebRequest&, WebResponse&)>;

	class WebServer
	{
	  public:
		WebServer();
		~WebServer();

		void start(uint16_t port, const char* web_root_folder, WebRequestHandler handler = nullptr);
		void stop();
		bool is_running() const;

	  private:
		static int callback(struct mg_connection* conn, void* user_data);

		mg_context* ctx;
		bool running;
		WebRequestHandler handler;
		FixedString512 document_root;
	};
} // namespace robotick
