// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
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

		enum class MessageType : uint8_t
		{
			Subscribe = 1,
			Ack = 2,
			Fields = 3
		};

		struct ConnectionConfig
		{
			std::string host;
			int port;
		};

		struct Field
		{
			std::string path;
			const void* send_ptr = nullptr;
			void* recv_ptr = nullptr;
			size_t size = 0;
			uint64_t type_hash = 0;
		};

		using BinderCallback = std::function<bool(const std::string& path, Field& out_field)>;

		RemoteEngineConnection(const ConnectionConfig& config, Mode mode);

		void tick();
		void cleanup();
		bool is_connected() const;
		bool is_ready_for_tick() const;

		void register_field(const Field& field);	  // for Proactive
		void set_field_binder(BinderCallback binder); // for Passive

	  private:
		void connect_socket();
		void accept_socket();
		void send_handshake();
		void receive_handshake_and_bind();
		void send_fields_as_message();
		void receive_into_fields();
		void send_message(const MessageType type, const uint8_t* data, size_t size);
		bool receive_message(std::vector<uint8_t>& buffer_out);

		void handle_handshake();
		void handle_tick_exchange();

		State state = State::Disconnected;
		Mode mode;
		ConnectionConfig config;

		int socket_fd = -1;
		std::vector<Field> fields;

		BinderCallback binder;
	};

} // namespace robotick
