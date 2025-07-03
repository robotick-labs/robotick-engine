// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/InProgressMessage.h"

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
		enum class Mode : uint8_t
		{
			Sender,
			Receiver
		};

		enum class State : uint8_t
		{
			Disconnected,
			ReadyForHandshake,
			ReadyForFieldsRequest,
			ReadyForFields
		};

		enum class MessageType : uint8_t
		{
			Subscribe = 1,
			FieldsRequest = 2,
			Fields = 3
		};

		enum class ReceiveResult : uint8_t
		{
			MessageReceiving,
			TryAgainNextTick,
			ConnectionLost
		};

		enum class SendResult : uint8_t
		{
			MessageSending,
			TryAgainNextTick,
			ConnectionLost
		};

		struct ConnectionConfig
		{
			FixedString64 host;
			int port;
		};

		struct Field
		{
			FixedString64 path;
			const void* send_ptr = nullptr;
			void* recv_ptr = nullptr;
			size_t size = 0;
			uint32_t type_hash;
		};

		using BinderCallback = std::function<bool(const char* path, Field& out_field)>;

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

		bool is_ready() const; // we have finished our handshake and ready for field-data exchange through out tick() method

	  private:
		[[nodiscard]] State get_state() const { return state; };
		void set_state(const State state);

		void tick_disconnected_sender();
		void tick_disconnected_receiver();
		void tick_send_handshake();
		void tick_receive_handshake_and_bind();
		void send_fields_as_message();
		void receive_into_fields();

		void tick_ready_for_handshake();
		void tick_ready_for_field_request();
		void tick_ready_for_fields();

		// things we set up once on startup:
		Mode mode;
		ConnectionConfig config;
		BinderCallback binder;

		// set on startup (register_field()) on Sender; on tick_receive_handshake_and_bind() on Receiver:
		std::vector<Field> fields;

		// runtime values:
		State state = State::Disconnected;
		int socket_fd = -1;
		float time_sec_to_reconnect = 0.0f;

		InProgressMessage in_progress_message;
	};

} // namespace robotick
