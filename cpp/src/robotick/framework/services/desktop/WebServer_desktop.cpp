// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/framework/services/WebServer.h"
#include "robotick/framework/strings/StringView.h"

#include <arpa/inet.h>
#include <civetweb.h>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace robotick
{
	bool WebSocketConnection::send_frame(const int opcode, const void* data, const size_t size) const
	{
		if (!conn)
		{
			return false;
		}
		struct mg_connection* ws_conn = static_cast<struct mg_connection*>(conn);
		mg_lock_connection(ws_conn);
		const int written = mg_websocket_write(
			ws_conn, opcode, static_cast<const char*>(data), size);
		mg_unlock_connection(ws_conn);
		return written >= 0;
	}

	bool WebSocketConnection::send_text(const char* text) const
	{
		if (!text)
		{
			return send_frame(MG_WEBSOCKET_OPCODE_TEXT, "", 0);
		}
		return send_frame(MG_WEBSOCKET_OPCODE_TEXT, text, ::strlen(text));
	}

	bool WebSocketConnection::send_binary(const void* data, const size_t size) const
	{
		return send_frame(MG_WEBSOCKET_OPCODE_BINARY, data, size);
	}

	struct WebServerImpl
	{
		mg_context* ctx = nullptr;
	};

	static void write_line(void* conn, const char* fmt, ...)
	{
		char buffer[512]; // stack, safe for ESP32 + desktop
		va_list args;
		va_start(args, fmt);
		int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		if (len > 0)
		{
			// clamp to buffer size
			if (static_cast<size_t>(len) > sizeof(buffer))
				len = sizeof(buffer);

			mg_write(static_cast<mg_connection*>(conn), buffer, len);
		}
	}

	static void write_raw(void* conn, const void* data, size_t size)
	{
		mg_write(static_cast<mg_connection*>(conn), data, size);
	}

	static const char* reason_phrase_for_status(const int status_code)
	{
		switch (status_code)
		{
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 204:
			return "No Content";
		case 400:
			return "Bad Request";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 409:
			return "Conflict";
		case 500:
			return "Internal Server Error";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		default:
			return "OK";
		}
	}

	// -------------------------------
	// WebResponse streaming operations
	// -------------------------------

	static inline void write_status_and_cors(void* conn, int status_code)
	{
		write_line(conn,
			"HTTP/1.1 %d %s\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Access-Control-Allow-Headers: Content-Type\r\n"
			"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
			"Access-Control-Expose-Headers: X-Robotick-Session-Id\r\n",
			status_code,
			reason_phrase_for_status(status_code));
	}

	void WebResponse::set_status_code(int code)
	{
		// header mutation after body is illegal
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		// first-touch of headers? print status + CORS now
		if (state == State::Start)
		{
			write_status_and_cors(conn, code);
			state = State::HeadersOpen;
		}

		status_code = code;
	}

	void WebResponse::set_content_type(const char* type)
	{
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		if (state == State::Start)
		{
			write_status_and_cors(conn, status_code);
			state = State::HeadersOpen;
		}

		content_type = type ? type : "text/plain";
	}

	void WebResponse::add_header(const char* header_line)
	{
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		if (state == State::Start)
		{
			write_status_and_cors(conn, status_code);
			state = State::HeadersOpen;
		}

		if (header_line && *header_line)
		{
			write_line(conn, "%s\r\n", header_line);
		}
	}

	void WebResponse::set_body(const void* data, size_t size)
	{
		ensure_body_allowed();

		if (state == State::Start)
		{
			// First emission: status + CORS first
			write_status_and_cors(conn, status_code);
			state = State::HeadersOpen;
		}

		if (state == State::HeadersOpen)
		{
			// Close the header block with CT + CL and the CRLFCRLF
			write_line(conn, "Content-Type: %s\r\n", content_type);
			write_line(conn, "Content-Length: %zu\r\n", size);
			write_line(conn, "\r\n");
			state = State::BodyStreaming;
		}
		// else BodyStreaming → (strict) you could fatal here if multiple body writes are disallowed.

		if (size > 0)
		{
			write_raw(conn, data, size);
		}
	}

	void WebResponse::set_body_string(const char* text)
	{
		set_body(text, strlen(text));
	}

	void WebResponse::finish()
	{
		if (state == State::Done)
			return;

		if (state == State::Start || state == State::HeadersOpen)
		{
			write_line(conn,
				"HTTP/1.1 %d OK\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: 0\r\n"
				"\r\n",
				status_code,
				content_type);
		}

		state = State::Done;
	}

	// -------------------------------------

	WebServer::WebServer()
	{
		impl = new WebServerImpl();
	}

	WebServer::~WebServer()
	{
		stop();
	}

	bool WebServer::is_running() const
	{
		return running;
	}

	static void parse_query_string(const char* qs, WebRequest& req)
	{
		if (!qs || !*qs)
			return;

		while (*qs && req.query_params.size() < req.query_params.capacity())
		{
			FixedString32 key;
			FixedString64 val;

			const char* k = qs;
			while (*qs && *qs != '=' && *qs != '&')
				++qs;
			key = FixedString32(k, qs - k);

			if (*qs == '=')
				++qs;

			const char* v = qs;
			while (*qs && *qs != '&')
				++qs;
			val = FixedString64(v, qs - v);

			req.query_params.add({key, val});

			if (*qs == '&')
				++qs;
		}
	}

	// ------------------------------
	// Main request handler
	// ------------------------------

	static int callback(struct mg_connection* c, void* userdata)
	{
		WebServer* self = static_cast<WebServer*>(userdata);
		if (!self || !self->get_handler())
			return 0;

		WebRequest req;
		const mg_request_info* info = mg_get_request_info(c);

		if (info->local_uri)
			req.uri = info->local_uri;
		if (info->request_method)
			req.method = info->request_method;
		if (info->query_string)
			parse_query_string(info->query_string, req);

		// Read body if present
		char buffer[1024];
		int len = mg_read(c, buffer, sizeof(buffer));
		if (len > 0)
			req.body.set_bytes(buffer, len);

		// File passthrough
		{
			FixedString512 full_path;
			const char* uri = req.uri.c_str();
			if (uri[0] == '/')
				uri++;

			full_path.format("%s/%s", self->get_document_root(), uri);

			FILE* f = ::fopen(full_path.c_str(), "rb");
			if (f)
			{
				::fclose(f);
				return 0;
			}
		}

		WebResponse resp;
		resp.conn = c;

		// User handlers follow a simple RAII rule: if they return true we finalize headers/body here (finish()),
		// otherwise CivetWeb continues searching for static files.  Centralizing `finish()` means handlers can
		// bail early without worrying about partially written responses.
		if (self->get_handler()(req, resp))
		{
			resp.finish();
			return resp.get_status_code();
		}

		return 0;
	}

	static const WebSocketEndpoint* find_websocket_endpoint(
		const WebServer* server, const char* uri)
	{
		if (!server || !uri)
		{
			return nullptr;
		}
		for (const auto& endpoint : server->get_websocket_endpoints())
		{
			if (StringView(uri).equals(endpoint.uri.c_str()))
			{
				return &endpoint;
			}
		}
		return nullptr;
	}

	static int websocket_connect_callback(const struct mg_connection* c, void* userdata)
	{
		WebServer* self = static_cast<WebServer*>(userdata);
		if (!self)
		{
			return 1;
		}
		const mg_request_info* info = mg_get_request_info(c);
		const char* local_uri = info ? info->local_uri : nullptr;
		const WebSocketEndpoint* endpoint = find_websocket_endpoint(self, local_uri);
		if (!endpoint)
		{
			return 1;
		}
		if (!endpoint->handler.on_connect)
		{
			return 0;
		}

		WebRequest req;
		req.method = "GET";
		if (local_uri)
		{
			req.uri = local_uri;
		}
		if (info && info->query_string)
		{
			parse_query_string(info->query_string, req);
		}

		WebSocketConnection ws_conn;
		ws_conn.conn = const_cast<mg_connection*>(c);
		const bool accepted = endpoint->handler.on_connect(req, ws_conn);
		return accepted ? 0 : 1;
	}

	static void websocket_ready_callback(struct mg_connection* c, void* userdata)
	{
		WebServer* self = static_cast<WebServer*>(userdata);
		if (!self)
		{
			return;
		}
		const mg_request_info* info = mg_get_request_info(c);
		const WebSocketEndpoint* endpoint = find_websocket_endpoint(
			self, info ? info->local_uri : nullptr);
		if (!endpoint || !endpoint->handler.on_ready)
		{
			return;
		}
		WebSocketConnection ws_conn;
		ws_conn.conn = c;
		endpoint->handler.on_ready(ws_conn);
	}

	static int websocket_data_callback(
		struct mg_connection* c,
		int bits,
		char* data,
		size_t data_len,
		void* userdata)
	{
		WebServer* self = static_cast<WebServer*>(userdata);
		if (!self)
		{
			return 0;
		}
		const mg_request_info* info = mg_get_request_info(c);
		const WebSocketEndpoint* endpoint = find_websocket_endpoint(
			self, info ? info->local_uri : nullptr);
		if (!endpoint || !endpoint->handler.on_message)
		{
			return 1;
		}
		WebSocketConnection ws_conn;
		ws_conn.conn = c;
		WebSocketMessage message;
		message.opcode = (bits & 0x0F);
		message.data = reinterpret_cast<const uint8_t*>(data);
		message.size = data_len;
		const bool keep_open = endpoint->handler.on_message(ws_conn, message);
		return keep_open ? 1 : 0;
	}

	static void websocket_close_callback(
		const struct mg_connection* c, void* userdata)
	{
		WebServer* self = static_cast<WebServer*>(userdata);
		if (!self)
		{
			return;
		}
		const mg_request_info* info = mg_get_request_info(c);
		const WebSocketEndpoint* endpoint = find_websocket_endpoint(
			self, info ? info->local_uri : nullptr);
		if (!endpoint || !endpoint->handler.on_close)
		{
			return;
		}
		WebSocketConnection ws_conn;
		ws_conn.conn = const_cast<mg_connection*>(c);
		endpoint->handler.on_close(ws_conn);
	}

	// ------------------------------

	void WebServer::start(const char* name, uint16_t port, const char* webroot, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer '%s' already running", name);
		// Passing port 0 delegates to CivetWeb/the OS to bind an available port deterministically.

		char exepath[512] = {};
		ssize_t len = readlink("/proc/self/exe", exepath, sizeof(exepath) - 1);
		if (len < 0)
			ROBOTICK_FATAL_EXIT("readlink failed");
		exepath[len] = '\0';

		for (ssize_t i = len - 1; i >= 0; --i)
			if (exepath[i] == '/')
			{
				exepath[i + 1] = '\0';
				break;
			}

		FixedString512 doc_root_candidate;
		if (webroot && *webroot)
			doc_root_candidate.format("%s%s", exepath, webroot);

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);

		char portstr[16];
		snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

		const char* opt_with[] = {"listening_ports", portstr, "document_root", doc_root_candidate.c_str(), "enable_directory_listing", "no", nullptr};
		const char* opt_no[] = {"listening_ports", portstr, "enable_directory_listing", "no", nullptr};
		const char** opts = doc_root_candidate.empty() ? opt_no : opt_with;

		s->ctx = mg_start(nullptr, nullptr, opts);
		if (!s->ctx)
		{
			ROBOTICK_FATAL_EXIT("Failed to start WebServer '%s' on port %u", name, (unsigned)port);
		}

		mg_server_port ports[1];
		if (mg_get_server_ports(s->ctx, 1, ports) <= 0 || ports[0].port == 0)
		{
			mg_stop(s->ctx);
			s->ctx = nullptr;
			ROBOTICK_FATAL_EXIT("Failed to determine bound port for WebServer '%s'", name);
		}
		bound_port = static_cast<uint16_t>(ports[0].port);

		mg_set_request_handler(s->ctx, "/", callback, this);
		for (const auto& endpoint : websocket_endpoints)
		{
			mg_set_websocket_handler(
				s->ctx,
				endpoint.uri.c_str(),
				websocket_connect_callback,
				websocket_ready_callback,
				websocket_data_callback,
				websocket_close_callback,
				this);
		}
		handler = handler_in;
		server_name = name;
		document_root = doc_root_candidate.c_str();
		running = true;
	}

	void WebServer::stop()
	{
		if (!running)
			return;

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);
		mg_stop(s->ctx);
		s->ctx = nullptr;

		running = false;
		bound_port = 0;
		handler = nullptr;
		server_name.clear();
		document_root.clear();
	}

	void WebServer::add_websocket_endpoint(const char* uri, WebSocketHandler ws_handler)
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer websocket endpoints must be added before start()");
		ROBOTICK_ASSERT_MSG(uri && uri[0] != '\0', "WebServer websocket endpoint uri must not be empty");
		ROBOTICK_ASSERT_MSG(!websocket_endpoints.full(), "WebServer websocket endpoint capacity exceeded");
		WebSocketEndpoint endpoint;
		endpoint.uri = uri;
		endpoint.handler = ws_handler;
		websocket_endpoints.add(endpoint);
	}

	void WebServer::clear_websocket_endpoints()
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer websocket endpoints cannot be cleared while running");
		websocket_endpoints.clear();
	}

} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
