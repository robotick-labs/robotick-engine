#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/common/Pair.h"

#include <cstdint>

namespace robotick
{
	using WebRequestBodyBuffer = FixedVector1k;

#if defined(ROBOTICK_PLATFORM_ESP32)
	using WebResponseBodyBuffer = FixedVector8k;
#else
	using WebResponseBodyBuffer = FixedVector256k; // big enough for a generous jpg image
#endif

	struct WebRequest
	{
		FixedString8 method;

		FixedString128 uri;
		FixedVector<Pair<FixedString32, FixedString64>, 8> query_params;

		WebRequestBodyBuffer body;

		const char* find_query_param(const char* key) const
		{
			for (const auto& pair : query_params)
			{
				if (pair.first == key)
					return pair.second.c_str();
			}
			return nullptr;
		}
	};

	struct WebResponseCode
	{
		static constexpr int OK = 200;
		static constexpr int Created = 201;
		static constexpr int NoContent = 204;

		static constexpr int BadRequest = 400;
		static constexpr int Unauthorized = 401;
		static constexpr int Forbidden = 403;
		static constexpr int NotFound = 404;

		static constexpr int MethodNotAllowed = 405;
		static constexpr int Conflict = 409;

		static constexpr int InternalServerError = 500;
		static constexpr int NotImplemented = 501;
		static constexpr int ServiceUnavailable = 503;
	};

	struct WebResponse
	{
		WebResponseBodyBuffer body;
		FixedString32 content_type = "text/plain"; // Default
		FixedVector<FixedString256, 64> headers;
		int status_code = WebResponseCode::NotFound; // Default: NotFound (intuituve - meaning it's not been handled)
	};

	using WebRequestHandler = std::function<bool(const WebRequest&, WebResponse&)>;

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
