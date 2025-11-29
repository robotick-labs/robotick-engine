#pragma once

#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/services/WebServer.h"

namespace robotick
{
	class Engine;

	class TelemetryServer
	{
	  public:
		TelemetryServer() = default;
		~TelemetryServer();

		void start(const Engine& engine, const uint16_t telemetry_port);
		void stop();

		const char* get_session_id() const { return session_id.c_str(); }

	  protected:
		void handle_get_home_page(const WebRequest&, WebResponse& res);
		void handle_get_workloads(const WebRequest& req, WebResponse& res);
		void handle_get_workload_config(const WebRequest& req, WebResponse& res);
		void handle_get_workload_inputs(const WebRequest& req, WebResponse& res);
		void handle_get_workload_outputs(const WebRequest& req, WebResponse& res);
		void handle_get_workload_output_png(const WebRequest& req, WebResponse& res);

		void handle_get_workload_io(const WebRequest& req, WebResponse& res, bool inputs);

		void handle_get_workloads_buffer_layout(const WebRequest& req, WebResponse& res);
		void handle_get_workloads_buffer_raw(const WebRequest& req, WebResponse& res);

	  private:
		WebServer web_server;

		const Engine* engine = nullptr;

		FixedString64 session_id;
	};
} // namespace robotick
