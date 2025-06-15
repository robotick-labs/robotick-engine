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
	struct TickInfo;

	class RemoteEngineConnection
	{
	  public:
		enum class Mode
		{
			Sender,
			Receiver
		};
		enum class State
		{
			Disconnected,
			ReadyForHandshake,
			ReadyForHandshakeAck,
			Ready
		};

		enum class MessageType : uint8_t
		{
			Subscribe = 1,
			Ack = 2,
			Fields = 3
		};

		enum class ReceiveResult
		{
			MessageReceived,
			NoMessageYet,
			ConnectionLost
		};

		enum class SendResult
		{
			Success,
			ConnectionLost
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

		RemoteEngineConnection() = default;
		~RemoteEngineConnection() noexcept { disconnect(); }

		RemoteEngineConnection(const RemoteEngineConnection&) = delete;
		RemoteEngineConnection& operator=(const RemoteEngineConnection&) = delete;
		RemoteEngineConnection(RemoteEngineConnection&&) noexcept = delete;
		RemoteEngineConnection& operator=(RemoteEngineConnection&&) noexcept = delete;

		void configure(const ConnectionConfig& config, Mode mode);

		void tick(const TickInfo& tick_info);

		void register_field(const Field& field);	  // for Sender
		void set_field_binder(BinderCallback binder); // for Receiver

		void disconnect();

		bool has_basic_connection() const; // we have established a basic connection, but perhaps but yet completed handshake
		bool is_ready() const;			   // we have finished our handshake and ready for field-data exchange through out tick() method

	  private:
		State get_state() const { return state; };
		void set_state(const State state);

		void connect_socket();
		void accept_socket();
		void send_handshake();
		void receive_handshake_and_bind();
		void send_fields_as_message();
		void receive_into_fields();

		SendResult send_message(const MessageType type, const uint8_t* data, size_t size);
		ReceiveResult receive_message(std::vector<uint8_t>& buffer_out);

		void handle_handshake();
		void try_receive_handshake_ack();
		void handle_tick_exchange();

	  private: // things we set up once on startup:
		Mode mode;
		ConnectionConfig config;
		BinderCallback binder;

	  private:
		std::vector<Field> fields; // set on startup (register_field()) on Sender; on receive_handshake_and_bind() on Receiver

	  private: // runtime values
		State state = State::Disconnected;
		int socket_fd = -1;
		float time_sec_to_reconnect = 0.0f;
	};

} // namespace robotick
