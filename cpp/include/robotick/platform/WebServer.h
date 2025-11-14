#pragma once

#include "robotick/api.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/common/Pair.h"

#include <cstdint>

namespace robotick
{
	using WebRequestBodyBuffer = FixedVector1k;

	struct WebRequest
	{
		FixedString8 method;
		FixedString128 uri;
		FixedVector<Pair<FixedString32, FixedString64>, 8> query_params;

		WebRequestBodyBuffer body;

		const char* find_query_param(const char* key) const
		{
			for (const auto& p : query_params)
			{
				if (p.first == key)
					return p.second.c_str();
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
		void* conn = nullptr; // platform-specific (mg_connection* or httpd_req_t*)

		enum class State
		{
			Start,
			HeadersOpen,
			BodyStreaming,
			Done
		};

		// --- ordering helpers ---
		inline void ensure_headers_open()
		{
			if (state == State::BodyStreaming || state == State::Done)
				ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
			if (state == State::Start)
				state = State::HeadersOpen;
		}

		inline void ensure_body_allowed()
		{
			if (state == State::Done)
				ROBOTICK_FATAL_EXIT("WebResponse: body written after finish()");
		}

		// --- header operations ---
		int get_status_code() const { return status_code; }
		void set_status_code(int code);

		void set_content_type(const char* type);
		void add_header(const char* header_line);

		// --- body operations ---
		void set_body(const void* data, size_t size);
		void set_body_string(const char* text);

		// --- finalise body, used if handler did nothing else ---
		void finish();

	  private:
		State state = State::Start;

		int status_code = WebResponseCode::OK;
		const char* content_type = "text/plain";
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

		WebRequestHandler get_handler() const { return handler; }
		const char* get_server_name() const { return server_name.c_str(); }
		const char* get_document_root() const { return document_root.c_str(); }

	  private:
		WebServerImpl* impl = nullptr;
		bool running = false;
		WebRequestHandler handler = nullptr;

		FixedString32 server_name;
		FixedString512 document_root;
	};
} // namespace robotick
