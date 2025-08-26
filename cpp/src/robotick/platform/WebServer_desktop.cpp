#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/WebServer.h"

#include <civetweb.h>
#include <cstdio>
#include <cstring>
#include <unistd.h> // for readlink

namespace robotick
{
	struct WebServerImpl
	{
		mg_context* ctx = nullptr;
	};

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

	void parse_query_string(const char* qs, WebRequest& request)
	{
		if (!qs || *qs == '\0')
			return;

		while (*qs && request.query_params.size() < request.query_params.capacity())
		{
			FixedString32 key;
			FixedString64 value;

			// Parse key
			const char* key_start = qs;
			while (*qs && *qs != '=' && *qs != '&')
				++qs;
			key = FixedString32(key_start, qs - key_start);

			if (*qs == '=')
				++qs;

			// Parse value
			const char* val_start = qs;
			while (*qs && *qs != '&')
				++qs;
			value = FixedString64(val_start, qs - val_start);

			request.query_params.add({key, value});

			if (*qs == '&')
				++qs;
		}
	}

	int web_server_callback(struct mg_connection* conn, void* user_data)
	{
		WebServer* self = static_cast<WebServer*>(user_data);
		if (!self || !self->get_handler())
			return 0;

		WebRequest request;
		const struct mg_request_info* req_info = mg_get_request_info(conn);

		if (req_info->local_uri)
			request.uri = req_info->local_uri;

		if (req_info->request_method)
			request.method = req_info->request_method;

		if (req_info->query_string)
			parse_query_string(req_info->query_string, request);

		// Read body if present
		char buffer[1024];
		int len = mg_read(conn, buffer, sizeof(buffer));
		if (len > 0)
			request.body.set(buffer, len);

		// Check if requested path exists as a file
		{
			FixedString512 file_path;

			const char* uri_path = request.uri.c_str();
			if (uri_path[0] == '/')
				++uri_path;

			file_path.format("%s/%s", self->get_document_root(), uri_path);

			FILE* f = std::fopen(file_path.c_str(), "rb");
			if (f)
			{
				std::fclose(f);
				return 0; // Let CivetWeb serve it
			}
		}

		WebResponse response;
		if (self->get_handler()(request, response))
		{
			mg_printf(conn,
				"HTTP/1.1 %d OK\r\n"
				"Access-Control-Allow-Origin: *\r\n"
				"Access-Control-Allow-Headers: Content-Type\r\n"
				"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %zu\r\n"
				"\r\n",
				response.status_code,
				response.content_type.c_str(),
				response.body.size());

			mg_write(conn, response.body.data(), response.body.size());

			constexpr bool verbose_http = false;
			if (verbose_http || response.status_code >= 400)
			{
				ROBOTICK_INFO("WebServer - %s %s - response code %i (%i bytes)",
					request.method.c_str(),
					request.uri.c_str(),
					response.status_code,
					(int)response.body.size());
			}

			return response.status_code;
		}

		return 0; // fallback (not handled - let CivetWeb handle the request)
	}

	void WebServer::start(const char* name, uint16_t port, const char* web_root_folder, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT_MSG(!running, "WebServer '%s' already started", name);

		server_name = name;

		handler = handler_in;

		// Read path to running executable
		char exe_path[512] = {};
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len < 0 || len >= static_cast<ssize_t>(sizeof(exe_path)))
		{
			ROBOTICK_FATAL_EXIT("Failed to read /proc/self/exe");
			return;
		}
		exe_path[len] = '\0';

		// Truncate to parent directory
		for (ssize_t i = len - 1; i >= 0; --i)
		{
			if (exe_path[i] == '/')
			{
				exe_path[i + 1] = '\0';
				break;
			}
		}

		// Append web_root_folder to base dir
		bool has_docroot = (web_root_folder && web_root_folder[0] != '\0');
		if (has_docroot)
		{
			document_root.format("%s%s", exe_path, web_root_folder);
		}
		else
		{
			document_root.clear();
		}

		char port_str[16];
		std::snprintf(port_str, sizeof(port_str), "%u", static_cast<unsigned int>(port));

		const char* options_with_docroot[] = {
			"listening_ports", port_str, "document_root", document_root.c_str(), "enable_directory_listing", "no", nullptr};
		const char* options_no_docroot[] = {"listening_ports", port_str, "enable_directory_listing", "no", nullptr};
		const char** options = has_docroot ? options_with_docroot : options_no_docroot;

		impl->ctx = mg_start(nullptr, nullptr, options);
		if (!impl->ctx)
		{
			ROBOTICK_FATAL_EXIT("WebServer '%s' failed to start CivetWeb on port %u", name, static_cast<unsigned int>(port));
			return;
		}

		mg_set_request_handler(impl->ctx, "/", &web_server_callback, this);
		running = true;

		ROBOTICK_INFO("WebServer '%s' serving from '%s' at http://localhost:%u",
			name,
			has_docroot ? document_root.c_str() : "<no files>",
			static_cast<unsigned int>(port));
	}

	void WebServer::stop()
	{
		if (!running)
			return;

		mg_stop(impl->ctx);
		impl->ctx = nullptr;
		running = false;

		ROBOTICK_INFO("WebServer stopped.");
	}
} // namespace robotick

#endif // ROBOTICK_PLATFORM_DESKTOP
