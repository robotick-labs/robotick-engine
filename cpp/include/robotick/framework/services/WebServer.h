// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/utility/Function.h"
#include "robotick/framework/utility/Pair.h"

#include <cstddef>
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
		static constexpr int PreconditionFailed = 412;
		static constexpr int InternalServerError = 500;
		static constexpr int NotImplemented = 501;
		static constexpr int ServiceUnavailable = 503;
	};

	struct WebResponse
	{
		friend class WebServer;

	  public:
		void* conn = nullptr; // platform-specific (mg_connection* or httpd_req_t*)

	  public:
		// header operations - do these first as needed (in order shown)
		int get_status_code() const { return status_code; }
		void set_status_code(int code);

		void set_content_type(const char* type);
		void add_header(const char* header_line);

		// body operations - do either one of these after headers (just once)
		void set_body(const void* data, size_t size);
		void set_body_string(const char* text);

		// calling of this is optional - it will otherwise get auto-called by WebServer code
		void finish();

	  protected:
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

	  private:
		State state = State::Start;

		int status_code = WebResponseCode::OK;
		FixedString64 status_line = "200 OK";
		const char* content_type = "text/plain";
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		bool write_failed = false;
#endif
	};

	using WebRequestHandler = Function<bool(const WebRequest&, WebResponse&)>;

	struct WebSocketMessage
	{
		int opcode = 0;
		const uint8_t* data = nullptr;
		size_t size = 0;
	};

	class WebSocketConnection
	{
	  public:
		void* conn = nullptr; // platform-specific (mg_connection* or httpd_req_t*)
		void* server = nullptr; // platform-specific (httpd_handle_t, unused on desktop)
		int socket_fd = -1; // used by platforms that identify sockets by fd

		bool is_valid() const { return conn != nullptr || (server != nullptr && socket_fd >= 0); }

		bool send_text(const char* text) const;
		bool send_binary(const void* data, size_t size) const;
		bool send_frame(int opcode, const void* data, size_t size) const;
	};

	struct WebSocketHandler
	{
		Function<bool(const WebRequest&, WebSocketConnection&)> on_connect = nullptr;
		Function<void(WebSocketConnection&)> on_ready = nullptr;
		Function<bool(WebSocketConnection&, const WebSocketMessage&)> on_message = nullptr;
		Function<void(const WebSocketConnection&)> on_close = nullptr;
	};

	struct WebSocketEndpoint
	{
		FixedString128 uri;
		WebSocketHandler handler = {};
	};

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
		uint16_t get_bound_port() const { return bound_port; }
		void add_websocket_endpoint(const char* uri, WebSocketHandler handler);
		void clear_websocket_endpoints();
		const FixedVector<WebSocketEndpoint, 8>& get_websocket_endpoints() const { return websocket_endpoints; }

	  private:
		WebServerImpl* impl = nullptr;
		bool running = false;
		WebRequestHandler handler = nullptr;
		uint16_t bound_port = 0;
		FixedString32 server_name;
		FixedString512 document_root;
		FixedVector<WebSocketEndpoint, 8> websocket_endpoints;
	};
} // namespace robotick
