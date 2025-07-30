#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/api.h"
#include "robotick/platform/WebServer.h"

#include <cstring>
#include <esp_http_server.h>
#include <esp_interface.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>

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

	static esp_err_t handle_http_request(httpd_req_t* req)
	{
		WebServerImpl* state = static_cast<WebServerImpl*>(req->user_ctx);
		if (!state || !state->handler)
			return ESP_FAIL;

		WebRequest request;
		WebResponse response;

		request.method = http_method_str((http_method)req->method);
		request.uri = req->uri;

		if (req->method == HTTP_POST && req->content_len > 0 && req->content_len < request.body.capacity())
		{
			int read = httpd_req_recv(req, reinterpret_cast<char*>(request.body.data()), req->content_len);
			if (read > 0)
				request.body.set_size(static_cast<size_t>(read));
		}

		state->handler(request, response);

		httpd_resp_set_type(req, response.content_type.c_str());
		httpd_resp_send(req, reinterpret_cast<const char*>(response.body.data()), response.body.size());

		ROBOTICK_INFO("WebServer - %s %s - response code %i (%i bytes)",
			request.method.c_str(),
			request.uri.c_str(),
			response.status_code,
			response.body.size());

		return ESP_OK;
	}

	void WebServer::start(const char* name, uint16_t port, const char* /*unused*/, WebRequestHandler handler_in)
	{
		ROBOTICK_ASSERT_MSG(!is_running(), "WebServer '%s' already running", name);

		server_name = name;

		WebServerImpl* state = static_cast<WebServerImpl*>(impl);
		state->handler = handler_in;

		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.stack_size = 12288; // Increase from default 4096 to 8KB
		config.server_port = port;
		config.uri_match_fn = httpd_uri_match_wildcard;

		if (httpd_start(&state->server, &config) != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("Failed to start ESP HTTP server '%s'", name);
			return;
		}

		httpd_uri_t handler_def = {.uri = "/*", .method = HTTP_GET, .handler = handle_http_request, .user_ctx = state};
		httpd_register_uri_handler(state->server, &handler_def);

		handler_def.method = HTTP_POST;
		httpd_register_uri_handler(state->server, &handler_def);

		// report success, including our url
		{
			esp_netif_ip_info_t ip_info;
			if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info) == ESP_OK)
			{
				ROBOTICK_INFO("WebServer '%s' serving at http://%d.%d.%d.%d:%u", name, IP2STR(&ip_info.ip), static_cast<unsigned int>(port));
			}
			else if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info) == ESP_OK)
			{
				ROBOTICK_INFO(
					"WebServer '%s' (AP mode) serving at http://%d.%d.%d.%d:%u", name, IP2STR(&ip_info.ip), static_cast<unsigned int>(port));
			}
			else
			{
				ROBOTICK_WARNING("WebServer '%s' started, but IP address is unknown", name);
			}
		}
	}

	void WebServer::stop()
	{
		WebServerImpl* state = static_cast<WebServerImpl*>(impl);
		if (!state->server)
			return;

		httpd_stop(state->server);
		state->server = nullptr;

		ESP_LOGI(TAG, "WebServer stopped.");
	}
} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32
