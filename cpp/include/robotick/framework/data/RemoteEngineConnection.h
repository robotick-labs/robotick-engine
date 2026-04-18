// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/containers/List.h"
#include "robotick/framework/data/InProgressMessage.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/utility/Function.h"

#include <cstdint>
#include <utility>

namespace robotick
{
	struct DataConnectionInputHandle;
	struct TickInfo;
	struct TypeDescriptor;

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

		struct Field
		{
			FixedString512 path;
			const void* send_ptr = nullptr;
			size_t size = 0;
			const TypeDescriptor* type_desc = nullptr;
			DataConnectionInputHandle* input_handle = nullptr;
		};

		using BinderCallback = Function<bool(const char* path, Field& out_field)>;

		RemoteEngineConnection() = default;
		~RemoteEngineConnection() noexcept { disconnect(); }

		RemoteEngineConnection(const RemoteEngineConnection&) = delete;
		RemoteEngineConnection& operator=(const RemoteEngineConnection&) = delete;
		RemoteEngineConnection(RemoteEngineConnection&&) noexcept = delete;
		RemoteEngineConnection& operator=(RemoteEngineConnection&&) noexcept = delete;

		void configure_sender(const char* in_my_model_name, const char* in_target_model_name, const char* in_remote_ip, uint16_t in_remote_port);
		void configure_receiver(const char* my_model_name);

		void tick(const TickInfo& tick_info);

		void register_field(const Field& field);	  // for Sender
		void set_field_binder(BinderCallback binder); // for Receiver

		void disconnect();

		[[nodiscard]] bool has_basic_connection() const; // we have established a basic connection, but perhaps but yet completed handshake

		[[nodiscard]] bool is_ready() const; // we have finished our handshake and ready for field-data exchange through out tick() method
		[[nodiscard]] uint16_t get_listen_port() const { return listen_port; }

#if defined(ROBOTICK_TEST_MODE)
		struct TestHealthLifecycleStats
		{
			uint32_t attempt_begin_count = 0;
			uint32_t healthy_transition_count = 0;
			uint32_t unhealthy_transition_count = 0;
		};

		[[nodiscard]] const TestHealthLifecycleStats& test_health_lifecycle_stats() const { return test_health_lifecycle_stats_; }
#endif

	  private:
		[[nodiscard]] State get_state() const { return state; };
		void set_state(const State state);

		size_t write_handshake_payload(uint32_t tick_rate_net, size_t offset, uint8_t* dst, size_t max_len) const;
		size_t write_fields_payload(size_t offset, uint8_t* dst, size_t max_len) const;

		void tick_disconnected_sender();
		void tick_disconnected_receiver();

		void tick_ready_for_handshake(const TickInfo& tick_info);
		void tick_sender_send_handshake(const TickInfo& tick_info);
		void tick_receiver_receive_handshake(const TickInfo& tick_info);

		void tick_send_fields_request(const bool allow_start_new);
		bool tick_receive_fields_request();

		void tick_send_fields_as_message(const bool allow_start_new);
		bool tick_receive_fields_as_message();
		void add_field(const Field& field, bool update_handshake_stats);
		void log_connection_attempt_begin_once();
		void log_connection_became_healthy();
		void log_connection_became_unhealthy();
		void schedule_reconnect_backoff();
		void reset_reconnect_backoff();
		[[nodiscard]] const char* get_mode_label() const;

	  private:
		// things we set up once on startup:
		Mode mode = Mode::Sender;

		FixedString64 my_model_name;	 // always set to name of host-model
		FixedString64 target_model_name; // empty on receivers; non-empty on senders

		// runtime endpoint data
		FixedString64 remote_ip;
		uint16_t remote_port = 0;

		// receiver's bound port (for announce)
		uint16_t listen_port = 0; // ask OS for ephemeral port first time; reuse assigned port thereafter

		BinderCallback binder;

		// Sender fields stay in a contiguous heap block for fast payload writes.
		// Receiver fields are accumulated incrementally during handshake, then copied
		// into this exact-sized buffer once binding is complete.
		HeapVector<Field> fields;
		List<Field> pending_receiver_fields;
		size_t field_count = 0;
		size_t handshake_path_total_length = 0;
		size_t handshake_payload_capacity = sizeof(uint32_t);
		size_t field_payload_capacity = 0;

		// runtime values:
		State state = State::Disconnected;
		int socket_fd = -1;
		float time_sec_to_reconnect = 0.0f;
		float reconnect_backoff_sec = 0.0f;
		bool connection_attempt_logged_for_current_outage = false;

		float mutual_tick_rate_hz = 0.0f; // gets set to minimum of receiver and sender engine's root tick-rate, on handshake
		uint64_t ticks_until_next_send = 1;

		// Each half of the TCP stream gets its own InProgressMessage so a long-running send never blocks an incoming reader.
		// This lets the tick loop pump READY + FIELD packets back-to-back without reentrancy hazards.
		InProgressMessage in_progress_message_in;
		InProgressMessage in_progress_message_out;

		// Persist incremental parsing across non-blocking recv for the handshake payload (tick-rate + paths)
		struct HandshakeReceiveState
		{
			uint8_t tick_rate_bytes[4]{};
			size_t tick_rate_bytes_received = 0;
			float sender_tick_rate_hz = 0.0f;
			FixedString512 current_path;
			size_t current_path_length = 0;
			size_t payload_bytes_consumed = 0;
			size_t bound_count = 0;
			size_t failed_count = 0;
		} handshake_receive_state;

		// Track streamed field payload placement across ticks while receiving field data
		struct FieldReceiveState
		{
			size_t field_index = 0;
			size_t offset_in_field = 0;
			size_t total_bytes_received = 0;
		} field_receive_state;

		// Capture mutual tick-rate bytes from FieldsRequest across partial reads
		struct TickRateReceiveState
		{
			uint8_t tick_rate_bytes[4]{};
			size_t tick_rate_bytes_received = 0;
			float tick_rate_hz = 0.0f;
		} fields_request_receive_state;

#if defined(ROBOTICK_TEST_MODE)
		TestHealthLifecycleStats test_health_lifecycle_stats_;
#endif
	};

} // namespace robotick
