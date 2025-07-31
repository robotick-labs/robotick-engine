#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/platform/WebServer.h"

namespace robotick
{
	class Engine;

	class TelemetryServer
	{
	  public:
		void start(const Engine& engine);
		void stop();

	  protected:
		void handle_get_home_page(const WebRequest&, WebResponse& res);
		void handle_get_workloads(const WebRequest& req, WebResponse& res);
		void handle_get_workload_stats(const WebRequest& req, WebResponse& res);
		void handle_get_workload_config(const WebRequest& req, WebResponse& res);
		void handle_get_workload_inputs(const WebRequest& req, WebResponse& res);
		void handle_get_workload_outputs(const WebRequest& req, WebResponse& res);

		void handle_get_workload_io(const WebRequest& req, WebResponse& res, bool inputs);

	  private:
		WebServer web_server;

		const Engine* engine = nullptr;
	};
} // namespace robotick
