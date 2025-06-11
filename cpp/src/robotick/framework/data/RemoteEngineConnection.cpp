// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include <stdexcept>

namespace robotick
{

	RemoteEngineConnection::RemoteEngineConnection(const std::string& uri, Mode mode) : mode(mode), uri(uri)
	{
	}

	void RemoteEngineConnection::set_remote_name(const std::string& name)
	{
		remote_name = name;
	}

	void RemoteEngineConnection::set_requested_fields(const std::vector<std::string>& fields)
	{
		requested_fields = fields;
	}

	void RemoteEngineConnection::set_available_outputs(const std::vector<std::string>& fields)
	{
		available_fields = fields;
	}

	void RemoteEngineConnection::tick()
	{
		if (!is_connected())
			open_socket();

		if (state == State::Connected)
			handle_handshake();

		if (state == State::Subscribed)
			handle_tick_exchange();
	}

	void RemoteEngineConnection::apply_received_data()
	{
		// TODO: parse and apply inbound_json
	}

	bool RemoteEngineConnection::is_connected() const
	{
		return state != State::Disconnected;
	}

	bool RemoteEngineConnection::is_ready_for_tick() const
	{
		return state == State::Ticking;
	}

	void RemoteEngineConnection::open_socket()
	{
		// TODO: TCP connect or listen/accept
		state = State::Connected;
	}

	void RemoteEngineConnection::handle_handshake()
	{
		// TODO: perform subscribe/ack logic
		state = State::Subscribed;
	}

	void RemoteEngineConnection::handle_tick_exchange()
	{
		// TODO: send outbound_json and receive inbound_json
		state = State::Ticking;
	}

	void RemoteEngineConnection::cleanup()
	{
		// TODO: close socket
		state = State::Disconnected;
	}

} // namespace robotick
