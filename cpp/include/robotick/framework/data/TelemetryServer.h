// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/utility/Function.h"

#include <cstdint>

namespace robotick
{
	class Engine;
	struct WebRequest;
	struct WebResponse;

	struct TelemetryServiceDescriptor
	{
		FixedString64 service_id;
		FixedString32 service_type;
		FixedString64 display_name;
		FixedVector<FixedString32, 8> capabilities;
	};

	using TelemetryServiceRequestHandler = Function<bool(const WebRequest&, WebResponse&, const char* relative_uri)>;

	class TelemetryServer
	{
	  public:
		TelemetryServer();
		~TelemetryServer();
		TelemetryServer(const TelemetryServer&) = delete;
		TelemetryServer(TelemetryServer&&) = delete;

		TelemetryServer& operator=(TelemetryServer&&) = delete;
		TelemetryServer& operator=(const TelemetryServer&) = delete;

		// core telemetry lifecycle management
		void setup(const Engine& engine);
		void start(const Engine& engine, const uint16_t telemetry_port);
		void stop();
		void apply_pending_input_writes();
		void update_peer_route(const char* model_id, const char* host, uint16_t telemetry_port, bool is_gateway);

		const char* get_session_id() const;

		// services lifecycle management
		bool register_service(const TelemetryServiceDescriptor& descriptor, TelemetryServiceRequestHandler handler);
		void clear_registered_services();

	  private:
		struct Impl;
		Impl* impl = nullptr;
	};
} // namespace robotick
