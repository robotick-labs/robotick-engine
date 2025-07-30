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
		void handle_get_workloads(const WebRequest& req, WebResponse& res);

	  private:
		WebServer web_server;

		const Engine* engine = nullptr;
	};
} // namespace robotick
