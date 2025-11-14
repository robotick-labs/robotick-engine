#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/api.h"
#include "robotick/platform/WebServer.h"

#include <cstring>
#include <esp_http_server.h>
#include <esp_log.h>

namespace robotick
{
	namespace
	{
		const char* TAG = "WebServer";
	}

	struct WebServerImpl
	{
		httpd_handle_t server = nullptr;
		WebRequestHandler handler = nullptr;
	};

	static void write_header(void* c, const char* key, const char* val)
	{
		httpd_req_t* req = static_cast<httpd_req_t*>(c);
		httpd_resp_set_hdr(req, key, val);
	}

	static void write_raw(void* c, const void* data, size_t sz)
	{
		httpd_req_t* req = static_cast<httpd_req_t*>(c);
		httpd_resp_send_chunk(req, (const char*)data, sz);
	}

	// -----------------------
	// WebResponse ESP32 impl
	// -----------------------

	void WebResponse::set_status_code(int code)
	{
		ensure_headers_open();
		status_code = code; // ESP32 doesn't send status until first body
	}

	void WebResponse::set_content_type(const char* type)
	{
		ensure_headers_open();
		content_type = type; // stored until first body write
	}

	void WebResponse::add_header(const char* header_line)
	{
		ensure_headers_open();

		// header_line: "X-Key: Value"
		const char* colon = strchr(header_line, ':');
		if (!colon)
			ROBOTICK_FATAL_EXIT("Invalid header_line");

		char key[128];
		size_t klen = colon - header_line;
		if (klen >= sizeof(key))
			klen = sizeof(key) - 1;

		memcpy(key, header_line, klen);
		key[klen] = '\0';

		const char* val = colon + 1;
		while (*val == ' ')
			val++;

		write_header(conn, key, val);
	}

	void WebResponse::set_body(const void* data, size_t size)
	{
		ensure_body_allowed();

		if (state == State::Start)
		{
			// first body write = finalize headers
			httpd_req_t* req = static_cast<httpd_req_t*>(conn);

			httpd_resp_set_status_code(req, (status_code == 200 ? "200 OK" : "200 OK")); // minimal
			httpd_resp_set_type(req, content_type);

			httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
			httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
			httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
			httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Robotick-Session-Id");

			state = State::BodyStreaming;
		}
		else if (state == State::HeadersOpen)
		{
			state = State::BodyStreaming;
		}

		write_raw(conn, data, size);
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
			// Handler returned true but never sent a body.
			// We must send proper headers first.
			httpd_req_t* req = static_cast<httpd_req_t*>(conn);

			httpd_resp_set_status(req, (status_code == 200 ? "200 OK" : "200 OK"));
			httpd_resp_set_type(req, content_type);

			httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
			httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
			httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
			httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Robotick-Session-Id");

			// Send an empty chunk as body
			httpd_resp_send_chunk(req, "", 0);
		}

		// Terminate chunked response
		httpd_resp_send_chunk(static_cast<httpd_req_t*>(conn), nullptr, 0);
		state = State::Done;
	}

	// ------------------------------
	// Main ESP32 request handler
	// ------------------------------

	static esp_err_t handler_fn(httpd_req_t* req)
	{
		WebServerImpl* s = static_cast<WebServerImpl*>(req->user_ctx);
		if (!s || !s->handler)
			return ESP_FAIL;

		WebRequest r;
		r.method = http_method_str((http_method)req->method);

		// Parse URI
		{
			const char* full = req->uri;
			const char* q = strchr(full, '?');
			if (q)
				r.uri = FixedString128(full, q - full);
			else
				r.uri = full;

			if (q)
			{
				const char* qs = q + 1;
				while (*qs)
				{
					// same parsing as desktop
					while (*qs == '&')
						++qs;
					if (!*qs)
						break;

					FixedString32 k;
					FixedString64 v;

					// parse key
					const char* ks = qs;
					while (*qs && *qs != '=' && *qs != '&')
						++qs;
					k = FixedString32(ks, qs - ks);
					if (*qs == '=')
						++qs;

					// parse val
					const char* vs = qs;
					while (*qs && *qs != '&')
						++qs;
					v = FixedString64(vs, qs - vs);

					r.query_params.add({k, v});

					if (*qs == '&')
						++qs;
				}
			}
		}

		// Read body
		if (req->content_len > 0 && req->content_len < r.body.capacity())
		{
			int got = httpd_req_recv(req, (char*)r.body.data(), req->content_len);
			if (got > 0)
				r.body.set_size(got);
		}

		WebResponse resp;
		resp.conn = req;

		if (s->handler(r, resp))
		{
			resp.finish();
			return ESP_OK;
		}
		else
		{
			httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
			return ESP_OK;
		}
	}

	// ------------------------------

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
		return static_cast<WebServerImpl*>(impl)->server != nullptr;
	}

	void WebServer::start(const char* name, uint16_t port, const char*, WebRequestHandler handler_in)
	{
		WebServerImpl* s = static_cast<WebServerImpl*>(impl);

		s->handler = handler_in;
		server_name = name;

		httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
		cfg.server_port = port;
		cfg.stack_size = 16384;
		cfg.uri_match_fn = httpd_uri_match_wildcard;

		if (httpd_start(&s->server, &cfg) != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("Failed to start ESP32 WebServer");
			return;
		}

		httpd_uri_t def = {.uri = "/*", .method = HTTP_GET, .handler = handler_fn, .user_ctx = s};
		httpd_register_uri_handler(s->server, &def);

		def.method = HTTP_POST;
		httpd_register_uri_handler(s->server, &def);
	}

	void WebServer::stop()
	{
		WebServerImpl* s = static_cast<WebServerImpl*>(impl);
		if (!s->server)
			return;

		httpd_stop(s->server);
		s->server = nullptr;
	}

} // namespace robotick

#endif // ESP32
