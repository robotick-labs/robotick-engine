// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

namespace robotick
{

	class RemoteEngineConnection
	{
	  public:
		enum class Mode
		{
			Proactive,
			Passive
		};
		enum class State
		{
			Disconnected,
			Connected,
			Subscribed,
			Ticking
		};

		RemoteEngineConnection(const std::string& uri, Mode mode);

		void set_remote_name(const std::string& name);
		void set_requested_fields(const std::vector<std::string>& fields);
		void set_available_outputs(const std::vector<std::string>& fields);

		void tick();
		void apply_received_data();

		bool is_connected() const;
		bool is_ready_for_tick() const;

	  private:
		void open_socket();
		void handle_handshake();
		void handle_tick_exchange();
		void cleanup();

		State state = State::Disconnected;
		Mode mode;

		int socket_fd = -1;
		std::string uri;
		std::string remote_name;
		std::vector<std::string> requested_fields;
		std::vector<std::string> available_fields;
		std::string inbound_json;
		std::string outbound_json;
	};

} // namespace robotick
