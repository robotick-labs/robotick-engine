// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <arpa/inet.h>
#include <csignal> // For signal(), SIGPIPE, SIG_IGN
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "esp_netif.h"
#endif

// =================================================================================================
//
// RemoteEngineConnection — One-to-One Engine Data Link
//
// This class enables structured data exchange between two Robotick engines over TCP.
// It handles:
//   - Connection setup (handshake)
//   - Field registration (sender-side)
//   - Field binding (receiver-side)
//   - Framed data transmission (via tick())
//
// Design Constraints:
//   • Each instance connects **to or from a single remote engine**.
//   • To support multiple peers, instantiate one sender or receiver per peer.
//
//   ➤ Sender: configure_sender(local_id, remote_id)
//     - Connects to one remote receiver.
//     - Sends a fixed set of fields.
//
//   ➤ Receiver: configure_receiver(local_id)
//     - Listens for one remote sender.
//     - Binds fields via a user-defined FieldBinder.
//
// This 1:1 design avoids multiplexing logic, simplifies routing, and ensures deterministic tick behavior.
// Use the Engine or a higher-level orchestration layer to manage multiple RemoteEngineConnections.
//
// =================================================================================================

#define ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE 0

namespace robotick
{
	namespace
	{
		constexpr float RECONNECT_ATTEMPT_INTERVAL_SEC = 0.01f;

		bool is_network_stack_ready()
		{
#if defined(ROBOTICK_PLATFORM_ESP32)
			esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
			return netif && esp_netif_is_netif_up(netif);
#else
			// Assume true on platforms like desktop
			return true;
#endif
		}

		int create_tcp_socket()
		{
			if (!is_network_stack_ready())
			{
				ROBOTICK_WARNING_ONCE("Network stack not ready — skipping socket creation");
				return -1;
			}

			int fd = socket(AF_INET, SOCK_STREAM, 0);
			if (fd < 0)
			{
				ROBOTICK_WARNING("Failed to create socket");
				return -1;
			}

			int opt = 1;

			// Allow address reuse
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			{
				close(fd);
				ROBOTICK_WARNING("Failed to set SO_REUSEADDR on socket");
			}

			// Disable Nagle's algorithm (batches up messages) — send every message immediately (essential for real-time control)
			if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
			{
				close(fd);
				ROBOTICK_WARNING("Failed to set TCP_NODELAY on socket");
			}

			// Set non-blocking
			int flags = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);

			return fd;
		}

	} // namespace

	void RemoteEngineConnection::set_state(const State target_state)
	{
		if (state == target_state)
		{
			return; // already in desired state
		}

		state = target_state;

		const bool is_receiver = (mode == Mode::Receiver);
		const char* mode_str = is_receiver ? "Receiver" : "Sender";
		const char* color_start = is_receiver ? "\033[33m" : "\033[32m"; // yellow : green
		const char* color_end = "\033[0m";

		if (state == State::Disconnected)
		{
			ROBOTICK_INFO("%s[%s] [-> State::Disconnected] - disconnected%s", color_start, mode_str, color_end);
		}
		else if (state == State::ReadyForHandshake)
		{
			ROBOTICK_INFO(
				"%s[%s] [-> State::ReadyForHandshake] - socket-connection established, ready for handshake%s", color_start, mode_str, color_end);
		}
		else if (state == State::ReadyForFields)
		{
			const char* field_data_str = is_receiver ? "receive" : "send";
			ROBOTICK_INFO("%s[%s] [-> State::ReadyForFields] - ready to %s fields-data!%s", color_start, mode_str, field_data_str, color_end);
		}
		else
		{
			ROBOTICK_FATAL_EXIT("[%s] - unknown state %i", mode_str, static_cast<int>(state));
		}
	}

	void RemoteEngineConnection::configure_sender(
		const char* in_my_model_name, const char* in_target_model_name, const char* in_remote_ip, uint16_t in_remote_port)
	{
		mode = Mode::Sender;
		my_model_name = in_my_model_name;
		target_model_name = in_target_model_name;
		remote_ip = in_remote_ip;
		remote_port = in_remote_port;

		set_state(State::Disconnected);
	}

	void RemoteEngineConnection::configure_receiver(const char* in_my_model_name)
	{
		mode = Mode::Receiver;
		my_model_name = in_my_model_name;
		target_model_name = ""; // receivers don't target
		listen_port = 0;		// we'll bind(0) and discover the port

		set_state(State::Disconnected);
	}

	// register_field() should be called on all sender connections - telling it which field(s) it should send
	// (we will treat any RemoteEngineConnection with no fields registered as a fatal-error - as its wasted and shouldn't exist)
	void RemoteEngineConnection::register_field(const Field& field)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::register_field() should only be called in Mode::Sender");

		fields.push_back(field);
	}

	// set_field_binder() should be called on all receiver connections - giving each a means of mapping (via its BinderCallback)
	// field-paths (received from remote-sender during handshake) to local field(s) by data-pointer that we can set directly)
	void RemoteEngineConnection::set_field_binder(BinderCallback binder_callback)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::set_field_binder() should only be called in Mode::Receiver");

		binder = std::move(binder_callback);
	}

	void RemoteEngineConnection::tick(const TickInfo& tick_info)
	{
		if (state == State::Disconnected)
		{
			if (time_sec_to_reconnect > 0.0F)
			{
				// wait a bit longer before trying to reconnect (try every RECONNECT_ATTEMPT_INTERVAL_SEC)
				time_sec_to_reconnect -= (float)tick_info.delta_time;
				return;
			}

			if (mode == Mode::Sender)
			{
				tick_disconnected_sender();
			}
			else
			{
				tick_disconnected_receiver();
			}

			if (state == State::Disconnected)
			{
				// still disconnected - try again in a bit
				time_sec_to_reconnect = RECONNECT_ATTEMPT_INTERVAL_SEC;
				return;
			}
			// else have an initial go at the below (fall-through)
		}
		if (state == State::ReadyForHandshake)
		{
			tick_ready_for_handshake(tick_info);
		}

		// TODO - make sense send a fields-message at mutual_tick_rate_hz - but once it's sent N in a row without a fields-request, it should pause
		// (lets say N=2 for now, since in theory we should really hear back from receiver (fields-request) every tick that we send a packet)
		if (state == State::ReadyForFields)
		{
			if (mode == Mode::Sender)
			{
				if (ticks_until_next_send > 0)
				{
					ticks_until_next_send -= 1;
					if (ticks_until_next_send == 0)
					{
						tick_send_fields_as_message(true);
					}
				}

				// sender would expect to need to receive ready-message, then immediately send fields-data
				if (tick_receive_fields_request())
				{
					// start sending one now; and schedule another for "ticks_until_next_send" time
					ticks_until_next_send = std::max((uint64_t)1, (uint64_t)(tick_info.tick_rate_hz / mutual_tick_rate_hz));
					ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "ticks_until_next_send: %i", (int)ticks_until_next_send);
					tick_send_fields_as_message(true);
				}

				tick_send_fields_as_message(false);
			}
			else // Mode::Receiver
			{
				// receiver would expect to need to receive fields, then immediately notify that its ready for next update

				bool any_received = false;
				while (tick_receive_fields_as_message())
				{
					any_received = true;
				}

				if (any_received)
				{
					tick_send_fields_request(true);
				}
				else
				{
					tick_send_fields_request(false);
				}
			}
		}
	}

	bool RemoteEngineConnection::has_basic_connection() const
	{
		return socket_fd >= 0 && state != State::Disconnected;
	}

	bool RemoteEngineConnection::is_ready() const
	{
		return state == State::ReadyForFields;
	}

	void RemoteEngineConnection::tick_disconnected_sender()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_disconnected_sender() should only be called in Mode::Sender");

		socket_fd = create_tcp_socket();
		if (socket_fd < 0)
		{
			return;
		}

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(remote_port);

		if (inet_pton(AF_INET, remote_ip.c_str(), &addr.sin_addr) != 1)
		{
			ROBOTICK_WARNING_ONCE("Invalid IP address: %s", remote_ip.c_str());
			close(socket_fd);
			socket_fd = -1;
			return;
		}

		if (connect(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
		{
			if (errno != EINPROGRESS)
			{
				ROBOTICK_WARNING("Failed to connect to %s:%d", remote_ip.c_str(), remote_port);
				close(socket_fd);
				socket_fd = -1;
				return;
			}
		}

		ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE,
			"Sender [%s] initiated connection to [%s] @ %s:%d",
			my_model_name.c_str(),
			target_model_name.c_str(),
			remote_ip.c_str(),
			remote_port);

		set_state(State::ReadyForHandshake);
	}

	void RemoteEngineConnection::tick_disconnected_receiver()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::tick_disconnected_receiver() should only be called in Mode::Receiver");

		if (socket_fd < 0)
		{
			socket_fd = create_tcp_socket();
			if (socket_fd < 0)
			{
				return;
			}

			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(listen_port);
			addr.sin_addr.s_addr = INADDR_ANY;

			if (bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
			{
				ROBOTICK_WARNING("Failed to bind socket");
				close(socket_fd);
				socket_fd = -1;
				return;
			}

			// 👇 Learn what port we were assigned
			sockaddr_in bound_addr{};
			socklen_t addr_len = sizeof(bound_addr);
			if (getsockname(socket_fd, (sockaddr*)&bound_addr, &addr_len) == 0)
			{
				listen_port = ntohs(bound_addr.sin_port);
				ROBOTICK_INFO("Receiver [%s] listening on port %d", my_model_name.c_str(), listen_port);
			}
			else
			{
				ROBOTICK_WARNING("Failed to get bound port");
			}

			if (listen(socket_fd, 1) < 0)
			{
				ROBOTICK_WARNING("Failed to listen on socket");
				close(socket_fd);
				socket_fd = -1;
				return;
			}

			return;
		}

		// Accept incoming connection
		sockaddr_in client_addr{};
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(socket_fd, (sockaddr*)&client_addr, &client_len);
		if (client_fd < 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				ROBOTICK_WARNING("Accept failed: errno=%d", errno);
			return;
		}

		int flags = fcntl(client_fd, F_GETFL, 0);
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

		close(socket_fd);
		socket_fd = client_fd;

		ROBOTICK_INFO("Receiver [%s] accepted connection on port %d", my_model_name.c_str(), listen_port);

		set_state(State::ReadyForHandshake);
	}

	void RemoteEngineConnection::tick_sender_send_handshake(const TickInfo& tick_info)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_sender_send_handshake() should only be called in Mode::Sender");

		if (fields.size() == 0)
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_sender_send_handshake() being called with no prior call(s) to "
								"RemoteEngineConnection::register_field()");
		}

		if (in_progress_message_out.is_vacant())
		{
			std::vector<uint8_t> payload;

			// add tick-rate:
			const float local_sender_tick_rate_hz = tick_info.tick_rate_hz;
			this->mutual_tick_rate_hz = local_sender_tick_rate_hz; // start off with our local tick-rate - this will get adjusted to mutual rate later
			payload.insert(payload.end(),
				reinterpret_cast<const uint8_t*>(&local_sender_tick_rate_hz),
				reinterpret_cast<const uint8_t*>(&local_sender_tick_rate_hz) + sizeof(local_sender_tick_rate_hz));

			// add fields info:
			bool is_first_field = true;
			for (const auto& field : fields)
			{
				if (field.path.contains('\n'))
				{
					ROBOTICK_FATAL_EXIT("Field path contains newline character - this will break handshake data: %s", field.path.c_str());
				}

				if (!is_first_field)
				{
					payload.push_back('\n');
				}
				payload.insert(payload.end(), &field.path.data[0], &field.path.data[0] + field.path.length());

				is_first_field = false;
			}

			in_progress_message_out.begin_send((uint8_t)MessageType::Subscribe, payload.data(), payload.size());
		}

		// pump non-blocking
		while (in_progress_message_out.is_occupied() && !in_progress_message_out.is_completed())
		{
			const InProgressMessage::Result r = in_progress_message_out.tick(socket_fd);
			if (r == InProgressMessage::Result::ConnectionLost)
			{
				ROBOTICK_WARNING("Connection lost sending handshake from Sender");
				disconnect();
				return;
			}
			if (r == InProgressMessage::Result::InProgress)
			{
				break; // would block
			}
		}

		if (in_progress_message_out.is_completed())
		{
			in_progress_message_out.vacate(); // vacate ready for next user

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender handshake sent with %zu field(s)", fields.size());
			set_state(State::ReadyForFields);
			ticks_until_next_send = 1;
			// ^- send first fields message immediately without being asked - this (together with the receiver sending its first 'ready'
			// 		message immediately too) will result in, if both engines running at same rate, field-updates being sent every tick.
		}
	}

	void RemoteEngineConnection::tick_receiver_receive_handshake(const TickInfo& tick_info)
	{
		(void)tick_info;

		ROBOTICK_ASSERT_MSG(
			mode == Mode::Receiver, "RemoteEngineConnection::tick_receiver_receive_handshake() should only be called in Mode::Receiver");

		if (!binder)
		{
			ROBOTICK_FATAL_EXIT("Receiver connection has no binder callback set before handshake");
		}

		if (in_progress_message_in.is_vacant())
		{
			in_progress_message_in.begin_receive();
		}

		// pump non-blocking
		while (in_progress_message_in.is_occupied() && !in_progress_message_in.is_completed())
		{
			const InProgressMessage::Result r = in_progress_message_in.tick(socket_fd);
			if (r == InProgressMessage::Result::ConnectionLost)
			{
				disconnect();
				return;
			}
			if (r == InProgressMessage::Result::InProgress)
			{
				break; // would block
			}
		}

		if (in_progress_message_in.is_completed())
		{
			auto [payload_data, payload_size] = in_progress_message_in.get_payload();

			const uint8_t* cursor = reinterpret_cast<const uint8_t*>(payload_data);
			const uint8_t* end = cursor + payload_size;

			// --- Parse float tick_rate (4 bytes) ---
			if (payload_size < sizeof(float))
			{
				ROBOTICK_FATAL_EXIT("Handshake payload too small (%lu) to contain tick_rate", payload_size);
			}

			float sender_tick_rate_hz = 0.0f;
			std::memcpy(&sender_tick_rate_hz, cursor, sizeof(float));
			cursor += sizeof(float);

			const float local_receiver_tick_rate_hz = tick_info.tick_rate_hz;
			this->mutual_tick_rate_hz = std::min(sender_tick_rate_hz, local_receiver_tick_rate_hz);

			if (!std::isfinite(sender_tick_rate_hz) || sender_tick_rate_hz <= 0.0f)
			{
				ROBOTICK_FATAL_EXIT("Invalid sender tick rate: %f", sender_tick_rate_hz);
			}

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender tick rate: %.2f Hz", sender_tick_rate_hz);

			// --- Parse newline-separated field paths from remaining bytes ---
			size_t bound_count = 0;
			size_t failed_count = 0;

			const char* field_data = reinterpret_cast<const char*>(cursor);
			const char* field_end = reinterpret_cast<const char*>(end);

			const char* line_start = field_data;

			for (const char* p = line_start; p <= field_end; ++p)
			{
				// Accept '\n' or end-of-buffer as delimiter
				if (p == field_end || *p == '\n')
				{
					size_t line_length = static_cast<size_t>(p - line_start);
					if (line_length > 0)
					{
						std::string path(line_start, line_length); // allocate only here
						Field field;
						if (!binder(path.c_str(), field))
						{
							ROBOTICK_WARNING("Failed to bind field: %s", path.c_str());
							failed_count++;
						}
						else
						{
							fields.push_back(field);
							bound_count++;
						}
					}
					line_start = p + 1;
				}
			}

			if (failed_count > 0)
			{
				ROBOTICK_FATAL_EXIT("Failed to bind %zu fields - disconnecting", failed_count);
			}

			in_progress_message_in.vacate(); // ready for next message

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE,
				"Receiver handshake received. Mutual tick-rate set to %.1f Hz. Bound %zu field(s) - total %zu (should be same value)",
				mutual_tick_rate_hz,
				bound_count,
				fields.size());

			set_state(State::ReadyForFields);
		}
	}

	void RemoteEngineConnection::tick_ready_for_handshake(const TickInfo& tick_info)
	{
		if (mode == Mode::Sender)
		{
			tick_sender_send_handshake(tick_info);
		}
		else
		{
			tick_receiver_receive_handshake(tick_info);
		}
	}

	void RemoteEngineConnection::tick_send_fields_request(const bool allow_start_new)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::tick_send_fields_request() should only be called in Mode::Receiver");

		// Send the FieldsRequest token + mutual tick-rate
		if (allow_start_new && in_progress_message_out.is_vacant())
		{
			float mutual_tick_rate = this->mutual_tick_rate_hz;
			static_assert(sizeof(float) == 4, "Expected float to be 4 bytes");

			std::vector<uint8_t> payload;
			const uint8_t* p = reinterpret_cast<const uint8_t*>(&mutual_tick_rate);
			payload.insert(payload.end(), p, p + sizeof(float));

			in_progress_message_out.begin_send((uint8_t)MessageType::FieldsRequest, payload.data(), payload.size());
		}

		// enhanced pump
		while (in_progress_message_out.is_occupied() && !in_progress_message_out.is_completed())
		{
			const InProgressMessage::Result tick_result = in_progress_message_out.tick(socket_fd);
			if (tick_result == InProgressMessage::Result::ConnectionLost)
			{
				ROBOTICK_WARNING("Connection lost sending field-request from Receiver");
				disconnect();
				return;
			}
			if (tick_result == InProgressMessage::Result::InProgress)
			{
				break; // would block
			}
		}

		if (in_progress_message_out.is_completed())
		{
			in_progress_message_out.vacate(); // vacate ready for next user
			set_state(State::ReadyForFields);
		}
	}

	bool RemoteEngineConnection::tick_receive_fields_request()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_receive_fields_request() should only be called in Mode::Sender");

		if (in_progress_message_in.is_vacant())
		{
			in_progress_message_in.begin_receive();
		}

		while (in_progress_message_in.is_occupied() && !in_progress_message_in.is_completed())
		{
			const InProgressMessage::Result tick_result = in_progress_message_in.tick(socket_fd);
			if (tick_result == InProgressMessage::Result::ConnectionLost)
			{
				ROBOTICK_WARNING("Connection lost receiving field-request from Receiver");
				disconnect();
				return false;
			}
			if (tick_result == InProgressMessage::Result::InProgress)
			{
				break; // would block
			}
		}

		if (in_progress_message_in.is_completed())
		{
			auto [payload_data, payload_size] = in_progress_message_in.get_payload();

			float received_mutual_tick_rate_hz = 0.0f;
			if (payload_size >= sizeof(float))
			{
				std::memcpy(&received_mutual_tick_rate_hz, payload_data, sizeof(float));
				if (std::isfinite(received_mutual_tick_rate_hz) && received_mutual_tick_rate_hz > 0.0f)
				{
					ROBOTICK_INFO_IF(
						ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender received mutual tick rate: %.2f Hz", received_mutual_tick_rate_hz);
					this->mutual_tick_rate_hz = received_mutual_tick_rate_hz;
				}
				else
				{
					ROBOTICK_WARNING("Invalid mutual tick rate received");
				}
			}
			else
			{
				ROBOTICK_WARNING("FieldsRequest missing mutual tick rate payload");
			}

			in_progress_message_in.vacate(); // vacate ready for next user
			return true;
		}

		return false;
	}

	void RemoteEngineConnection::tick_send_fields_as_message(const bool allow_start_new)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_send_fields_as_message() should only be called in Mode::Sender");

		if (allow_start_new && in_progress_message_out.is_vacant())
		{
			std::vector<uint8_t> buffer;
			for (const auto& field : fields)
			{
				const uint8_t* ptr = reinterpret_cast<const uint8_t*>(field.send_ptr);
				if (!ptr)
					continue;
				buffer.insert(buffer.end(), ptr, ptr + field.size);
			}

			in_progress_message_out.begin_send((uint8_t)MessageType::Fields, buffer.data(), buffer.size());
		}

		const InProgressMessage::Result tick_result = in_progress_message_out.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost sending field-data from Sender");
			disconnect();
			return;
		}

		if (in_progress_message_out.is_completed())
		{
			in_progress_message_out.vacate(); // vacate ready for next user
		}
	}

	bool RemoteEngineConnection::tick_receive_fields_as_message()
	{
		ROBOTICK_ASSERT_MSG(
			mode == Mode::Receiver, "RemoteEngineConnection::tick_receive_fields_as_message() should only be called in Mode::Receiver");

		if (in_progress_message_in.is_vacant())
		{
			in_progress_message_in.begin_receive();
		}

		const InProgressMessage::Result tick_result = in_progress_message_in.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost receiving field-data from Sender");
			disconnect();
			return false;
		}

		if (!in_progress_message_in.is_completed())
		{
			// not finished yet (would block) — try again next tick
			return false;
		}

		// process message
		auto [payload_data, payload_size] = in_progress_message_in.get_payload();

		size_t offset_into_payload = 0;
		for (auto& field : fields)
		{
			if (offset_into_payload + field.size > payload_size)
			{
				ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_receive_fields_as_message() - buffer received is too small (%zu bytes) for all "
									"expected fields (%zu)",
					payload_size,
					(offset_into_payload + field.size));

				break;
			}

			if (!field.recv_ptr)
			{
				ROBOTICK_FATAL_EXIT("Receiver field '%s' has null recv_ptr", field.path.c_str());
			}

			std::memcpy(field.recv_ptr, payload_data + offset_into_payload, field.size);
			offset_into_payload += field.size;

			static bool s_enable_debug_info = false;
			if (s_enable_debug_info)
			{
				ROBOTICK_INFO("Successfully written %zu bytes into field '%s'", field.size, field.path.c_str());
			}
		}

		in_progress_message_in.vacate(); // vacate ready for next user
		return true;
	}

	void RemoteEngineConnection::disconnect()
	{
		// tick both in_progress_message's if occupied, to ensure we're not sending partial messages
		InProgressMessage* in_progress_messages[] = {&in_progress_message_in, &in_progress_message_out};

		for (InProgressMessage* in_progress_message_ptr : in_progress_messages)
		{
			InProgressMessage& in_progress_message = *in_progress_message_ptr;

			constexpr int max_wait_ms = 500;
			constexpr int tick_wait_ms = 10;
			int total_wait_ms = 0;
			while (socket_fd >= 0 && total_wait_ms <= max_wait_ms && in_progress_message.is_occupied() && !in_progress_message.is_completed())
			{
				InProgressMessage::Result result = in_progress_message.tick(socket_fd);
				if (result == InProgressMessage::Result::ConnectionLost)
				{
					break;
				}

				Thread::sleep_ms(tick_wait_ms);
				total_wait_ms += tick_wait_ms;
			}

			in_progress_message.vacate();
		}

		if (socket_fd >= 0)
			close(socket_fd);
		socket_fd = -1;

		time_sec_to_reconnect = RECONNECT_ATTEMPT_INTERVAL_SEC;

		if (mode == Mode::Receiver)
		{
			// receiver gets told what fields to use by sender, on handshake.  We should therefore clear then whenever we disconnect
			fields.clear();
		}

		set_state(State::Disconnected);
	}

} // namespace robotick
