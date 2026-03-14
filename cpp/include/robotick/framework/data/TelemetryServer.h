// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{
	class Engine;

	class TelemetryServer
	{
	  public:
		TelemetryServer();
		~TelemetryServer();
		TelemetryServer(const TelemetryServer&) = delete;
		TelemetryServer(TelemetryServer&&) = delete;

		TelemetryServer& operator=(TelemetryServer&&) = delete;
		TelemetryServer& operator=(const TelemetryServer&) = delete;

		void setup(const Engine& engine);
		void start(const Engine& engine, const uint16_t telemetry_port);
		void stop();
		void apply_pending_input_writes();
		void update_peer_route(const char* model_id, const char* host, uint16_t telemetry_port, bool is_gateway);

		const char* get_session_id() const;

	  private:
		struct Impl;
		Impl* impl = nullptr;
	};
} // namespace robotick
