#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/WebServer.h"

#include <civetweb.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace robotick
{
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

	// -------------------------------
	// WebResponse streaming operations
	// -------------------------------

	static inline void write_status_and_cors(void* conn, int status_code)
	{
		write_line(conn,
			"HTTP/1.1 %d OK\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Access-Control-Allow-Headers: Content-Type\r\n"
			"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
			"Access-Control-Expose-Headers: X-Robotick-Session-Id\r\n",
			status_code);
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

		if (self->get_handler()(req, resp))
		{
			resp.finish();
			return resp.get_status_code();
		}

		return 0;
	}

	// ------------------------------

	void WebServer::start(const char* name, uint16_t port, const char* webroot, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer '%s' already running", name);

		server_name = name;
		handler = handler_in;

		// Resolve executable path
		char exepath[512] = {};
		ssize_t len = readlink("/proc/self/exe", exepath, sizeof(exepath) - 1);
		if (len < 0)
			ROBOTICK_FATAL_EXIT("readlink failed");
		exepath[len] = '\0';

		// Trim filename from path
		for (ssize_t i = len - 1; i >= 0; --i)
			if (exepath[i] == '/')
			{
				exepath[i + 1] = '\0';
				break;
			}

		if (webroot && *webroot)
			document_root.format("%s%s", exepath, webroot);
		else
			document_root.clear();

		char portstr[16];
		snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

		const char* opt_with[] = {"listening_ports", portstr, "document_root", document_root.c_str(), "enable_directory_listing", "no", nullptr};

		const char* opt_no[] = {"listening_ports", portstr, "enable_directory_listing", "no", nullptr};

		const char** opts = (webroot && *webroot) ? opt_with : opt_no;

		WebServerImpl* s = static_cast<WebServerImpl*>(impl);
		s->ctx = mg_start(nullptr, nullptr, opts);
		if (!s->ctx)
			ROBOTICK_FATAL_EXIT("Failed to start WebServer");

		mg_set_request_handler(s->ctx, "/", callback, this);
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
	}

} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
