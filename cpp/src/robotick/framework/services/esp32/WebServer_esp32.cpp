// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#if defined(ROBOTICK_PLATFORM_ESP32S3)

#include "robotick/api.h"
#include "robotick/framework/services/WebServer.h"

#include <cstring>
#include <esp_http_server.h>

namespace robotick
{
	struct WebServerImpl
	{
		httpd_handle_t handle = nullptr;
	};

	// ----------------------------------------
	//  Write utilities
	// ----------------------------------------

	static inline void write_status_and_cors(httpd_req_t* req, int status_code)
	{
		char status_line[32];
		snprintf(status_line, sizeof(status_line), "%d OK", status_code);
		httpd_resp_set_status(req, status_line);

		httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
		httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
		httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
		httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Robotick-Session-Id");
	}

	static inline void write_chunk(httpd_req_t* req, const void* data, size_t size)
	{
		httpd_resp_send_chunk(req, (const char*)data, size);
	}

	static inline void write_empty_terminator(httpd_req_t* req)
	{
		httpd_resp_send_chunk(req, nullptr, 0);
	}

	// ----------------------------------------
	//  WebResponse streaming operations (ESP32)
	// ----------------------------------------

	void WebResponse::set_status_code(int code)
	{
		// Illegal after body start
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		ensure_headers_open();
		status_code = code;
	}

	void WebResponse::set_content_type(const char* type)
	{
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		ensure_headers_open();
		content_type = type ? type : "text/plain";
	}

	void WebResponse::add_header(const char* header_line)
	{
		if (state == State::BodyStreaming || state == State::Done)
		{
			ROBOTICK_FATAL_EXIT("WebResponse: header call after body started");
		}

		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		if (state == State::Start)
		{
			write_status_and_cors(req, status_code);
			state = State::HeadersOpen;
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

			httpd_resp_set_hdr(req, key.c_str(), val);
		}
	}

	void WebResponse::set_body(const void* data, size_t size)
	{
		ensure_body_allowed();
		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		// First time writing any data? Emit status + CORS + type
		if (state == State::Start)
		{
			write_status_and_cors(req, status_code);
			httpd_resp_set_type(req, content_type);
			state = State::HeadersOpen;
		}

		// If headers are open and body is starting, close header block
		if (state == State::HeadersOpen)
		{
			// Done setting headers. From now on, only chunked body.
			state = State::BodyStreaming;
		}

		// Body write
		if (size > 0)
		{
			write_chunk(req, data, size);
		}
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

		httpd_req_t* req = static_cast<httpd_req_t*>(conn);

		// No body ever written → send an empty body with correct headers
		if (state == State::Start || state == State::HeadersOpen)
		{
			write_status_and_cors(req, status_code);
			httpd_resp_set_type(req, content_type);

			// Empty chunk
			write_chunk(req, "", 0);
		}

		// Terminate chunked transfer
		write_empty_terminator(req);

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
			size_t to_read = req->content_len;
			char buf[1024];
			int rd = httpd_req_recv(req, buf, to_read);
			if (rd > 0)
			{
				r.body.set_bytes(buf, rd);
			}
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

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);

		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.server_port = port;
		config.uri_match_fn = httpd_uri_match_wildcard;

		if (httpd_start(&s->handle, &config) != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("Failed to start ESP32 WebServer");
		}

		httpd_uri_t get_uri = {.uri = "/*", .method = HTTP_GET, .handler = esp32_handler, .user_ctx = this};
		httpd_uri_t post_uri = {.uri = "/*", .method = HTTP_POST, .handler = esp32_handler, .user_ctx = this};
		httpd_uri_t options_uri = {.uri = "/*", .method = HTTP_OPTIONS, .handler = esp32_handler, .user_ctx = this};

		httpd_register_uri_handler(s->handle, &get_uri);
		httpd_register_uri_handler(s->handle, &post_uri);
		httpd_register_uri_handler(s->handle, &options_uri);

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

} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32S3
