// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#if defined(ROBOTICK_PLATFORM_ESP32S3)

#include "robotick/api.h"
#include "robotick/framework/services/WebServer.h"

#include <cstdio>
#include <cstring>
#include <esp_http_server.h>

namespace robotick
{
	namespace
	{
#if defined(CONFIG_HTTPD_WS_SUPPORT)
		static httpd_ws_type_t map_ws_opcode_to_esp(const int opcode)
		{
			switch (opcode)
			{
			case 0x1:
				return HTTPD_WS_TYPE_TEXT;
			case 0x2:
				return HTTPD_WS_TYPE_BINARY;
			case 0x8:
				return HTTPD_WS_TYPE_CLOSE;
			case 0x9:
				return HTTPD_WS_TYPE_PING;
			case 0xA:
				return HTTPD_WS_TYPE_PONG;
			default:
				return HTTPD_WS_TYPE_BINARY;
			}
		}

		static int map_esp_opcode_to_ws(const httpd_ws_type_t type)
		{
			switch (type)
			{
			case HTTPD_WS_TYPE_TEXT:
				return 0x1;
			case HTTPD_WS_TYPE_BINARY:
				return 0x2;
			case HTTPD_WS_TYPE_CLOSE:
				return 0x8;
			case HTTPD_WS_TYPE_PING:
				return 0x9;
			case HTTPD_WS_TYPE_PONG:
				return 0xA;
			default:
				return 0x2;
			}
		}
#endif
	} // namespace

	bool WebSocketConnection::send_frame(const int opcode, const void* data, const size_t size) const
	{
#if defined(CONFIG_HTTPD_WS_SUPPORT)
		httpd_ws_frame_t frame = {};
		frame.type = map_ws_opcode_to_esp(opcode);
		frame.payload = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data));
		frame.len = size;

		if (conn)
		{
			return httpd_ws_send_frame(static_cast<httpd_req_t*>(conn), &frame) == ESP_OK;
		}
		if (server && socket_fd >= 0)
		{
			return httpd_ws_send_frame_async(static_cast<httpd_handle_t>(server), socket_fd, &frame) == ESP_OK;
		}
#else
		(void)opcode;
		(void)data;
		(void)size;
#endif
		return false;
	}

	bool WebSocketConnection::send_text(const char* text) const
	{
		if (!text)
		{
			return send_frame(0x1, "", 0);
		}
		return send_frame(0x1, text, ::strlen(text));
	}

	bool WebSocketConnection::send_binary(const void* data, const size_t size) const
	{
		return send_frame(0x2, data, size);
	}

	struct WebServerImpl
	{
		httpd_handle_t handle = nullptr;
#if defined(CONFIG_HTTPD_WS_SUPPORT)
		struct WsEndpointContext
		{
			WebServer* server = nullptr;
			size_t endpoint_index = 0;
		};
		WsEndpointContext ws_contexts[8] = {};
		httpd_uri_t ws_handlers[8] = {};
		size_t ws_handler_count = 0;
#endif
	};

	namespace
	{
		const char* get_reason_phrase(const int status_code)
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
			case 401:
				return "Unauthorized";
			case 403:
				return "Forbidden";
			case 404:
				return "Not Found";
			case 405:
				return "Method Not Allowed";
			case 409:
				return "Conflict";
			case 412:
				return "Precondition Failed";
			case 500:
				return "Internal Server Error";
			case 501:
				return "Not Implemented";
			case 503:
				return "Service Unavailable";
			default:
				return "OK";
			}
		}
	} // namespace

	// ----------------------------------------
	//  Write utilities
	// ----------------------------------------

	static inline bool write_status_and_cors(httpd_req_t* req, const char* status_line)
	{
		if (!req || !status_line)
			return false;

		if (httpd_resp_set_status(req, status_line) != ESP_OK)
			return false;

		if (httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*") != ESP_OK)
			return false;
		if (httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type") != ESP_OK)
			return false;
		if (httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS") != ESP_OK)
			return false;
		if (httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Robotick-Session-Id") != ESP_OK)
			return false;

		return true;
	}

	static inline bool write_content_type(httpd_req_t* req, const char* type)
	{
		return req && httpd_resp_set_type(req, type ? type : "text/plain") == ESP_OK;
	}

	static inline bool write_header(httpd_req_t* req, const char* key, const char* value)
	{
		return req && httpd_resp_set_hdr(req, key, value) == ESP_OK;
	}

	static inline bool write_full_body(httpd_req_t* req, const void* data, size_t size)
	{
		if (!req)
			return false;
		const char* body = static_cast<const char*>(data);
		if (!body)
			body = "";
		return httpd_resp_send(req, body, size) == ESP_OK;
	}

	// ----------------------------------------
	//  WebResponse streaming operations (ESP32)
	// ----------------------------------------

	void WebResponse::set_status_code(int code)
	{
		if (write_failed)
			return;
		ensure_headers_open();
		status_code = code;
		char formatted_status_line[64] = {};
		snprintf(formatted_status_line, sizeof(formatted_status_line), "%d %s", status_code, get_reason_phrase(status_code));
		status_line = formatted_status_line;
	}

	void WebResponse::set_content_type(const char* type)
	{
		if (write_failed)
			return;
		ensure_headers_open();
		content_type = type ? type : "text/plain";
	}

	void WebResponse::add_header(const char* header_line)
	{
		if (write_failed)
			return;

		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		if (state == State::Start)
		{
			if (!write_status_and_cors(req, status_line.c_str()))
			{
				write_failed = true;
				state = State::Done;
				return;
			}
		}

		ensure_headers_open();

		if (header_line && *header_line)
		{
			// Add raw header line
			// ESP-IDF requires key:value form
			const char* colon = strchr(header_line, ':');
			if (!colon)
			{
				ROBOTICK_FATAL_EXIT("WebResponse: add_header requires 'Key: Value'");
			}
			FixedString32 key(header_line, colon - header_line);
			const char* val = colon + 1;
			while (*val == ' ')
				val++;

			if (!write_header(req, key.c_str(), val))
			{
				write_failed = true;
				state = State::Done;
			}
		}
	}

	void WebResponse::set_body(const void* data, size_t size)
	{
		if (write_failed)
			return;

		ensure_body_allowed();
		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		// Apply current status/headers before sending the one-shot body.
		if (state == State::Start || state == State::HeadersOpen)
		{
			if (!write_status_and_cors(req, status_line.c_str()) || !write_content_type(req, content_type))
			{
				write_failed = true;
				state = State::Done;
				return;
			}
		}

		if (!write_full_body(req, data, size))
		{
			write_failed = true;
			state = State::Done;
			return;
		}

		state = State::Done;
	}

	void WebResponse::set_body_string(const char* text)
	{
		set_body(text, text ? strlen(text) : 0);
	}

	// Called after handler returns
	void WebResponse::finish()
	{
		if (state == State::Done)
			return;

		if (write_failed)
		{
			state = State::Done;
			return;
		}

		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		// No body ever written → send an empty response with correct headers
		if (state == State::Start || state == State::HeadersOpen)
		{
			if (!write_status_and_cors(req, status_line.c_str()) || !write_content_type(req, content_type))
			{
				write_failed = true;
				state = State::Done;
				return;
			}
			if (!write_full_body(req, "", 0))
			{
				write_failed = true;
			}
		}

		state = State::Done;
	}

	// ----------------------------------------
	//  Main request handler glue
	// ----------------------------------------

	static esp_err_t esp32_handler(httpd_req_t* req)
	{
		WebServer* self = static_cast<WebServer*>(req->user_ctx);
		if (!self || !self->get_handler())
			return ESP_OK;

		WebRequest r;

		const char* full_uri = req->uri;
		const char* query_start = strchr(full_uri, '?');

		if (query_start)
		{
			r.uri = FixedString128(full_uri, query_start - full_uri);
		}
		else
		{
			r.uri = full_uri;
		}

		// Method
		if (req->method == HTTP_GET)
			r.method = "GET";
		else if (req->method == HTTP_POST)
			r.method = "POST";
		else if (req->method == HTTP_OPTIONS)
			r.method = "OPTIONS";
		else
			r.method = "OTHER";

		// Query string parsing
		if (query_start)
		{
			const char* qs = query_start + 1;
			while (*qs && r.query_params.size() < r.query_params.capacity())
			{
				FixedString32 key;
				FixedString64 val;

				const char* k = qs;
				while (*qs && *qs != '=' && *qs != '&')
					qs++;
				key = FixedString32(k, qs - k);

				if (*qs == '=')
					qs++;

				const char* v = qs;
				while (*qs && *qs != '&')
					qs++;
				val = FixedString64(v, qs - v);

				r.query_params.add({key, val});

				if (*qs == '&')
					qs++;
			}
		}

		// Body read (ESP32 doesn't support chunked POST in this setup)
		if (req->content_len > 0)
		{
			if (req->content_len > static_cast<int>(r.body.capacity()))
			{
				httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request body too large");
				return ESP_OK;
			}

			size_t total_read = 0;
			while (total_read < static_cast<size_t>(req->content_len))
			{
				const size_t remaining = static_cast<size_t>(req->content_len) - total_read;
				const int rd = httpd_req_recv(req, reinterpret_cast<char*>(r.body.data()) + total_read, static_cast<int>(remaining));
				if (rd <= 0)
				{
					httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request body");
					return ESP_OK;
				}
				total_read += static_cast<size_t>(rd);
			}
			r.body.set_size(total_read);
		}

		// Dynamic handler, skip static file logic (ESP-side)
		WebResponse resp;
		resp.conn = req;

		// The handler owns the response lifetime: on true we finalize/flush once here so callers cannot accidentally
		// leak chunked transfers or leave sockets half-written.
		bool handled = self->get_handler()(r, resp);
		if (handled)
		{
			resp.finish();
			return ESP_OK;
		}

		// Not handled → 404
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
		return ESP_OK;
	}

#if defined(CONFIG_HTTPD_WS_SUPPORT)
	static esp_err_t esp32_ws_handler(httpd_req_t* req)
	{
		WebServerImpl::WsEndpointContext* context =
			static_cast<WebServerImpl::WsEndpointContext*>(req->user_ctx);
		if (!context || !context->server)
		{
			return ESP_FAIL;
		}
		const auto& endpoints = context->server->get_websocket_endpoints();
		if (context->endpoint_index >= endpoints.size())
		{
			return ESP_FAIL;
		}
		const WebSocketEndpoint& endpoint = endpoints[context->endpoint_index];

		WebSocketConnection ws_conn;
		ws_conn.conn = req;
		ws_conn.server = req->handle;
		ws_conn.socket_fd = httpd_req_to_sockfd(req);

		if (req->method == HTTP_GET)
		{
			WebRequest ws_req;
			ws_req.method = "GET";
			ws_req.uri = req->uri ? req->uri : "";

			if (endpoint.handler.on_connect)
			{
				if (!endpoint.handler.on_connect(ws_req, ws_conn))
				{
					return ESP_FAIL;
				}
			}
			if (endpoint.handler.on_ready)
			{
				endpoint.handler.on_ready(ws_conn);
			}
			return ESP_OK;
		}

		httpd_ws_frame_t frame = {};
		frame.type = HTTPD_WS_TYPE_TEXT;
		esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
		if (err != ESP_OK)
		{
			return err;
		}

		FixedVector8k payload_buffer;
		if (frame.len > payload_buffer.capacity())
		{
			return ESP_FAIL;
		}
		if (frame.len > 0)
		{
			payload_buffer.set_size(frame.len);
			frame.payload = payload_buffer.begin();
			err = httpd_ws_recv_frame(req, &frame, frame.len);
			if (err != ESP_OK)
			{
				return err;
			}
		}

		if (endpoint.handler.on_message)
		{
			WebSocketMessage ws_message;
			ws_message.opcode = map_esp_opcode_to_ws(frame.type);
			ws_message.data = frame.len > 0 ? payload_buffer.begin() : nullptr;
			ws_message.size = frame.len;
			const bool keep_open = endpoint.handler.on_message(ws_conn, ws_message);
			if (!keep_open)
			{
				if (endpoint.handler.on_close)
				{
					endpoint.handler.on_close(ws_conn);
				}
				return ESP_FAIL;
			}
		}
		return ESP_OK;
	}
#endif

	// ----------------------------------------
	//  WebServer lifecycle
	// ----------------------------------------

	WebServer::WebServer()
	{
		impl = new WebServerImpl();
	}

	WebServer::~WebServer()
	{
		stop();
		delete static_cast<WebServerImpl*>(impl);
		impl = nullptr;
	}

	bool WebServer::is_running() const
	{
		return running;
	}

	void WebServer::start(const char* name, uint16_t port, const char* webroot, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer '%s' already running", name);

		server_name = name;
		handler = handler_in;
		(void)webroot; // Static files not supported on ESP32; parameter intentionally ignored.

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);

		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.server_port = port;
		config.uri_match_fn = httpd_uri_match_wildcard;
		config.max_open_sockets = 7;
		config.lru_purge_enable = true;
		config.task_caps = MALLOC_CAP_8BIT;
		config.stack_size = 12288;

		const esp_err_t start_err = httpd_start(&s->handle, &config);
		if (start_err != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("Failed to start ESP32 WebServer: %s", esp_err_to_name(start_err));
		}

		httpd_uri_t get_uri = {.uri = "/*", .method = HTTP_GET, .handler = esp32_handler, .user_ctx = this};
		httpd_uri_t post_uri = {.uri = "/*", .method = HTTP_POST, .handler = esp32_handler, .user_ctx = this};
		httpd_uri_t options_uri = {.uri = "/*", .method = HTTP_OPTIONS, .handler = esp32_handler, .user_ctx = this};

		httpd_register_uri_handler(s->handle, &get_uri);
		httpd_register_uri_handler(s->handle, &post_uri);
		httpd_register_uri_handler(s->handle, &options_uri);

#if defined(CONFIG_HTTPD_WS_SUPPORT)
		s->ws_handler_count = 0;
		for (size_t i = 0; i < websocket_endpoints.size(); ++i)
		{
			if (s->ws_handler_count >= 8)
			{
				ROBOTICK_WARNING("WebServer websocket endpoint capacity exceeded on ESP32; skipping endpoint %zu", i);
				break;
			}
			WebServerImpl::WsEndpointContext& context = s->ws_contexts[s->ws_handler_count];
			context.server = this;
			context.endpoint_index = i;

			httpd_uri_t& ws_uri = s->ws_handlers[s->ws_handler_count];
			ws_uri = {};
			ws_uri.uri = websocket_endpoints[i].uri.c_str();
			ws_uri.method = HTTP_GET;
			ws_uri.handler = esp32_ws_handler;
			ws_uri.user_ctx = &context;
			ws_uri.is_websocket = true;
			ws_uri.handle_ws_control_frames = true;

			const esp_err_t register_err = httpd_register_uri_handler(s->handle, &ws_uri);
			if (register_err != ESP_OK)
			{
				ROBOTICK_WARNING("Failed to register websocket endpoint '%s' (err=%s)",
					websocket_endpoints[i].uri.c_str(),
					esp_err_to_name(register_err));
				continue;
			}
			s->ws_handler_count += 1;
		}
#endif

		running = true;
	}

	void WebServer::stop()
	{
		if (!running)
			return;

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);
		if (s->handle)
		{
			httpd_stop(s->handle);
			s->handle = nullptr;
		}

		running = false;
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

#endif // ROBOTICK_PLATFORM_ESP32S3
