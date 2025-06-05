#include "robotick/platform/WebServer.h"
#include "robotick/api.h"

#include <civetweb.h>
#include <filesystem>

namespace robotick
{
	WebServer::WebServer() : ctx(nullptr), running(false)
	{
	}

	WebServer::~WebServer()
	{
		stop();
	}

	bool WebServer::is_running() const
	{
		return running;
	}

	int WebServer::callback(struct mg_connection* conn, void* user_data)
	{
		WebServer* self = static_cast<WebServer*>(user_data);
		if (!self)
			return 0;

		WebRequest request;
		const struct mg_request_info* req_info = mg_get_request_info(conn);
		request.uri = req_info->local_uri ? req_info->local_uri : "";
		request.method = req_info->request_method ? req_info->request_method : "";

		// Read body if present (e.g. for POST)
		char buffer[1024];
		int len = mg_read(conn, buffer, sizeof(buffer));
		if (len > 0)
			request.body = std::string(buffer, len);

		// Try static file path
		std::filesystem::path file_path = std::filesystem::path(self->document_root) / request.uri.substr(1); // Strip leading '/'
		if (std::filesystem::exists(file_path))
		{
			return 0; // Let CivetWeb handle it
		}

		if (!self->handler)
			return 0;

		WebResponse response;
		try
		{
			self->handler(request, response);
		}
		catch (const std::exception& e)
		{
			ROBOTICK_WARNING("[WebServer] Exception in fallback handler for %s: %s", std::string(request.uri).c_str(), e.what());
			response.body = "[WebServer] Handler error";
			response.status_code = 500; // Internal Server Error
		}

		mg_printf(conn,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n",
			response.content_type.c_str(), response.body.size());
		mg_write(conn, response.body.data(), response.body.size());

		return response.status_code;
	}

	void WebServer::start(uint16_t port, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT(!running && "WebServer already started");
		handler = std::move(handler_in);

		std::filesystem::path exe = std::filesystem::canonical("/proc/self/exe");
		std::filesystem::path base_dir = exe.parent_path() / "data/remote_control_interface_web";
		document_root = base_dir.string();
		std::string port_str = std::to_string(port);

		const char* options[] = {
			"listening_ports", port_str.c_str(), "document_root", document_root.c_str(), "enable_directory_listing", "no", nullptr};

		ROBOTICK_ASSERT_MSG(std::filesystem::exists(base_dir / "index.html"), "Expected index.html not found in %s", document_root.c_str());

		ctx = mg_start(nullptr, nullptr, options);
		if (!ctx)
		{
			ROBOTICK_FATAL_EXIT("WebServer failed to start CivetWeb on port %d", port);
			return;
		}

		// Register fallback handler on `/` so it's considered for all requests
		mg_set_request_handler(ctx, "/", &WebServer::callback, this);
		running = true;

		ROBOTICK_INFO("WebServer serving from '%s' at http://localhost:%d", document_root.c_str(), port);
	}

	void WebServer::stop()
	{
		if (!running)
			return;

		mg_stop(ctx);
		ctx = nullptr;
		running = false;

		ROBOTICK_INFO("WebServer stopped.");
	}
} // namespace robotick
