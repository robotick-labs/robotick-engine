// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/math/IsFinite.h"

#include <arpa/inet.h>
#include <csignal> // For signal(), SIGPIPE, SIG_IGN
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(ROBOTICK_PLATFORM_ESP32S3)
#include "esp_netif.h"
#endif

// =================================================================================================
//
// RemoteEngineConnection — One-to-One Engine Data Link
//
// This class enables structured, rate-aware data exchange between two Robotick engines over TCP.
// It supports real-time field updates with explicit pacing control and automatic synchronization.
//
// Responsibilities:
//   - Connection setup and handshake
//   - Field registration (Sender side)
//   - Field binding (Receiver side)
//   - Framed data transmission via `tick()` loop
//   - Mutual tick-rate negotiation (prevents flooding or stalling)
//
// Protocol Overview:
//   • One side acts as Sender, one as Receiver
//   • Sender announces its tick-rate during handshake
//   • Receiver replies with the mutual tick-rate (min of both) each fields-request
//   • Thereafter:
//       - Receiver sends READY (field-request) messages at mutual rate
//       - Sender may transmit one or more FIELD messages per READY
//       - Receiver consumes all incoming FIELDs before next READY
//
// Design Constraints:
//   • Each RemoteEngineConnection links exactly one sender to one receiver
//   • To support multiple peers, instantiate multiple connections
//   • No dynamic field mapping: all fields are fixed at startup
//
// Why this design?
//   • No polling, no global clocks — just pacing via mutual tick-rate
//   • Works even if sender runs faster than receiver (e.g. 500Hz → 30Hz)
//   • Keeps latency low, avoids buffer overflows, enables debugging
//   • All handshake and pacing logic happens inside `tick()`
//
//
// =================================================================================================

#define ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE 0

namespace robotick
{
	namespace
	{
		constexpr float RECONNECT_ATTEMPT_INTERVAL_SEC = 0.01f;
		constexpr size_t MAX_REMOTE_FIELDS = 128;

		template <typename T> inline T rtk_min(const T a, const T b)
		{
			return (a < b) ? a : b;
		}

		template <typename T> inline T rtk_max(const T a, const T b)
		{
			return (a > b) ? a : b;
		}

		bool is_network_stack_ready()
		{
#if defined(ROBOTICK_PLATFORM_ESP32S3)
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

		// Pushing all log output + colorization through this gate keeps every transition auditable; callers simply request
		// the new state and let the centralized logger/TCP cleanup run.  That in turn makes RAII cleanup (close sockets on stop)
		// far easier to reason about.
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

		add_field(field, true);
	}

	// set_field_binder() should be called on all receiver connections - giving each a means of mapping (via its BinderCallback)
	// field-paths (received from remote-sender during handshake) to local field(s) by data-pointer that we can set directly)
	void RemoteEngineConnection::set_field_binder(BinderCallback binder_callback)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::set_field_binder() should only be called in Mode::Receiver");

		binder = binder_callback;
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

			// Connection state machine deliberately tears down and recreates sockets per transition.  Rebinding avoids
			// reusing half-closed FDs and guarantees both sender/receiver restart cleanly after network failures.
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
					const float mr = (mutual_tick_rate_hz > 0.0f) ? mutual_tick_rate_hz : tick_info.tick_rate_hz;
					ticks_until_next_send = rtk_max<uint64_t>(1, (uint64_t)::floor(tick_info.tick_rate_hz / mr));
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
					// Keep consuming all pending field messages before issuing a new FieldsRequest.
					// This prevents stalling when senders burst multiple packets between ticks.
					// Especially important if the sender is running at a higher tick-rate.

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

	static inline uint32_t float_to_network_bytes(float value)
	{
		uint32_t as_int = 0;
		::memcpy(&as_int, &value, sizeof(float));
		return htonl(as_int); // convert to network byte order (big-endian)
	}

	static inline float network_bytes_to_float(uint32_t net_bytes)
	{
		uint32_t host_order = ntohl(net_bytes);
		float value = 0.0f;
		::memcpy(&value, &host_order, sizeof(float));
		return value;
	}

	void RemoteEngineConnection::add_field(const Field& field, bool update_handshake_stats)
	{
		if (fields.size() == 0)
		{
			fields.initialize(MAX_REMOTE_FIELDS);
		}

		if (field_count >= fields.size())
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection exceeded max fields capacity (%zu)", fields.size());
		}

		fields[field_count] = field;
		field_count += 1;
		field_payload_capacity += field.size;

		if (update_handshake_stats)
		{
			handshake_path_total_length += field.path.length();
			const size_t separator_count = (field_count > 0) ? (field_count - 1) : 0;
			handshake_payload_capacity = sizeof(uint32_t) + handshake_path_total_length + separator_count;
		}
	}

	size_t RemoteEngineConnection::write_handshake_payload(uint32_t tick_rate_net, size_t offset, uint8_t* dst, size_t max_len) const
	{
		const uint8_t tick_bytes[sizeof(uint32_t)] = {static_cast<uint8_t>(tick_rate_net >> 24),
			static_cast<uint8_t>((tick_rate_net >> 16) & 0xFF),
			static_cast<uint8_t>((tick_rate_net >> 8) & 0xFF),
			static_cast<uint8_t>(tick_rate_net & 0xFF)};

		size_t written = 0;
		size_t cursor = offset;

		if (cursor < sizeof(tick_bytes))
		{
			const size_t take = rtk_min(max_len, sizeof(tick_bytes) - cursor);
			memcpy(dst, tick_bytes + cursor, take);
			written += take;
			cursor += take;
		}

		if (written == max_len)
			return written;

		if (cursor < sizeof(tick_bytes))
			return written;

		size_t paths_offset = cursor - sizeof(tick_bytes);
		size_t remaining = max_len - written;
		size_t skip = paths_offset;

		for (size_t i = 0; i < field_count && remaining > 0; ++i)
		{
			const auto& field = fields[i];
			const size_t path_len = field.path.length();

			if (skip >= path_len)
			{
				skip -= path_len;
			}
			else
			{
				const size_t take = rtk_min(path_len - skip, remaining);
				memcpy(dst + written, field.path.data + skip, take);
				written += take;
				remaining -= take;
				skip = 0;
			}

			if (remaining == 0)
				break;

			if (i + 1 < field_count)
			{
				if (skip > 0)
				{
					skip -= 1;
				}
				else
				{
					dst[written++] = '\n';
					remaining -= 1;
				}
			}
		}

		return written;
	}

	size_t RemoteEngineConnection::write_fields_payload(size_t offset, uint8_t* dst, size_t max_len) const
	{
		size_t written = 0;
		size_t skip = offset;

		for (size_t i = 0; i < field_count; ++i)
		{
			const auto& field = fields[i];
			const uint8_t* src = reinterpret_cast<const uint8_t*>(field.send_ptr);
			if (skip >= field.size)
			{
				skip -= field.size;
				continue;
			}

			const size_t take = rtk_min(field.size - skip, max_len - written);
			if (src)
			{
				memcpy(dst + written, src + skip, take);
			}
			else
			{
				memset(dst + written, 0, take);
			}
			written += take;
			skip = 0;

			if (written >= max_len)
				break;
		}

		return written;
	}

	void RemoteEngineConnection::tick_sender_send_handshake(const TickInfo& tick_info)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_sender_send_handshake() should only be called in Mode::Sender");

		if (field_count == 0)
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_sender_send_handshake() being called with no prior call(s) to "
								"RemoteEngineConnection::register_field()");
		}

		if (in_progress_message_out.is_vacant())
		{
			// The sender announces its local tick-rate (Hz) in the Subscribe message.
			// The receiver will min() this with its own rate and echo the result
			// in the first FieldsRequest — establishing a mutual tick-rate.
			// This enables smooth pacing and avoids receiver overrun.

			const float local_sender_tick_rate_hz = tick_info.tick_rate_hz;
			this->mutual_tick_rate_hz = local_sender_tick_rate_hz; // start off with our local tick-rate - this will get adjusted to mutual rate later
			const uint32_t tick_rate_net = float_to_network_bytes(local_sender_tick_rate_hz);

			for (size_t i = 0; i < field_count; ++i)
			{
				const auto& field = fields[i];
				if (field.path.contains('\n'))
				{
					ROBOTICK_FATAL_EXIT("Field path contains newline character - this will break handshake data: %s", field.path.c_str());
				}
			}

			auto writer = [this, tick_rate_net](size_t offset, uint8_t* dst, size_t max_len) -> size_t
			{
				return write_handshake_payload(tick_rate_net, offset, dst, max_len);
			};

			in_progress_message_out.begin_send((uint8_t)MessageType::Subscribe, handshake_payload_capacity, writer);
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

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender handshake sent with %zu field(s)", field_count);
			set_state(State::ReadyForFields);

			// Emit first fields-message immediately to establish mutual pacing promptly.
			tick_send_fields_as_message(true);
		}
	}

	void RemoteEngineConnection::tick_receiver_receive_handshake(const TickInfo& tick_info)
	{
		ROBOTICK_ASSERT_MSG(
			mode == Mode::Receiver, "RemoteEngineConnection::tick_receiver_receive_handshake() should only be called in Mode::Receiver");

		if (!binder)
		{
			ROBOTICK_FATAL_EXIT("Receiver connection has no binder callback set before handshake");
		}

		if (in_progress_message_in.is_vacant())
		{
			handshake_receive_state = {};
			field_count = 0;
			field_payload_capacity = 0;
			handshake_path_total_length = 0;

			auto reader = [this](const uint8_t* data, size_t len)
			{
				size_t consumed = 0;
				handshake_receive_state.payload_bytes_consumed += len;

				auto flush_current_path = [this]()
				{
					if (handshake_receive_state.current_path_length == 0)
						return;

					if (handshake_receive_state.current_path_length >= handshake_receive_state.current_path.capacity())
					{
						ROBOTICK_FATAL_EXIT("Field path too long (%zu chars): exceeds handshake buffer", handshake_receive_state.current_path_length);
					}

					handshake_receive_state.current_path.data[handshake_receive_state.current_path_length] = '\0';

					Field field;
					if (!binder(handshake_receive_state.current_path.c_str(), field))
					{
						ROBOTICK_WARNING("Failed to bind field: %s", handshake_receive_state.current_path.c_str());
						handshake_receive_state.failed_count++;
					}
					else
					{
						add_field(field, false);
						handshake_receive_state.bound_count++;
					}

					handshake_receive_state.current_path_length = 0;
					handshake_receive_state.current_path.data[0] = '\0';
				};

				// First 4 bytes are tick-rate
				while (handshake_receive_state.tick_rate_bytes_received < sizeof(uint32_t) && consumed < len)
				{
					handshake_receive_state.tick_rate_bytes[handshake_receive_state.tick_rate_bytes_received++] = data[consumed++];

					if (handshake_receive_state.tick_rate_bytes_received == sizeof(uint32_t))
					{
						uint32_t tick_rate_net = 0;
						memcpy(&tick_rate_net, handshake_receive_state.tick_rate_bytes, sizeof(uint32_t));
						handshake_receive_state.sender_tick_rate_hz = network_bytes_to_float(tick_rate_net);
					}
				}

				// Remainder is newline-separated field paths
				while (consumed < len)
				{
					const char c = static_cast<char>(data[consumed++]);
					if (c == '\n')
					{
						flush_current_path();
						continue;
					}

					if (handshake_receive_state.current_path_length + 1 >= handshake_receive_state.current_path.capacity())
					{
						ROBOTICK_FATAL_EXIT(
							"Field path too long (%zu chars): exceeds handshake buffer", handshake_receive_state.current_path_length + 1);
					}

					handshake_receive_state.current_path.data[handshake_receive_state.current_path_length++] = c;
				}

				// Trailing path flushed after full payload.
			};

			in_progress_message_in.begin_receive(reader);
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
			const size_t expected_tick_bytes = sizeof(uint32_t);
			if (handshake_receive_state.tick_rate_bytes_received < expected_tick_bytes)
			{
				ROBOTICK_FATAL_EXIT("Handshake payload too small to contain tick_rate");
			}

			// Flush final path if no trailing newline
			if (handshake_receive_state.current_path_length > 0)
			{
				if (handshake_receive_state.current_path_length >= handshake_receive_state.current_path.capacity())
				{
					ROBOTICK_FATAL_EXIT("Field path too long (%zu chars): exceeds handshake buffer", handshake_receive_state.current_path_length);
				}

				handshake_receive_state.current_path.data[handshake_receive_state.current_path_length] = '\0';

				Field field;
				if (!binder(handshake_receive_state.current_path.c_str(), field))
				{
					ROBOTICK_WARNING("Failed to bind field: %s", handshake_receive_state.current_path.c_str());
					handshake_receive_state.failed_count++;
				}
				else
				{
					add_field(field, false);
					handshake_receive_state.bound_count++;
				}
				handshake_receive_state.current_path_length = 0;
				handshake_receive_state.current_path.data[0] = '\0';
			}

			const size_t reported_payload = in_progress_message_in.payload_length();
			const size_t actual_payload = handshake_receive_state.payload_bytes_consumed;
			if (reported_payload != actual_payload)
			{
				ROBOTICK_FATAL_EXIT(
					"Handshake payload length mismatch: header reports %zu bytes but processed %zu", reported_payload, actual_payload);
			}

			const float sender_tick_rate_hz = handshake_receive_state.sender_tick_rate_hz;

			if (!robotick::isfinite(sender_tick_rate_hz) || sender_tick_rate_hz <= 0.0f)
			{
				ROBOTICK_FATAL_EXIT("Invalid sender tick rate: %f", sender_tick_rate_hz);
			}

			const float local_receiver_tick_rate_hz = tick_info.tick_rate_hz;
			this->mutual_tick_rate_hz = rtk_min(sender_tick_rate_hz, local_receiver_tick_rate_hz);

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender tick rate: %.2f Hz", sender_tick_rate_hz);

			if (handshake_receive_state.failed_count > 0)
			{
				ROBOTICK_FATAL_EXIT("Failed to bind %zu fields - disconnecting", handshake_receive_state.failed_count);
			}

			in_progress_message_in.vacate(); // ready for next message

			ROBOTICK_INFO_IF(ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE,
				"Receiver handshake received. Mutual tick-rate set to %.1f Hz. Bound %zu field(s) - total %zu (should be same value)",
				mutual_tick_rate_hz,
				handshake_receive_state.bound_count,
				field_count);

			set_state(State::ReadyForFields);

			// Emit first READY immediately to establish mutual pacing promptly.
			tick_send_fields_request(true);
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

			uint32_t tick_rate_net = float_to_network_bytes(mutual_tick_rate);

			auto writer = [tick_rate_net](size_t offset, uint8_t* dst, size_t max_len) -> size_t
			{
				const uint8_t bytes[sizeof(uint32_t)] = {static_cast<uint8_t>(tick_rate_net >> 24),
					static_cast<uint8_t>((tick_rate_net >> 16) & 0xFF),
					static_cast<uint8_t>((tick_rate_net >> 8) & 0xFF),
					static_cast<uint8_t>(tick_rate_net & 0xFF)};

				if (offset >= sizeof(bytes) || max_len == 0)
					return 0;

				const size_t take = rtk_min(max_len, sizeof(bytes) - offset);
				memcpy(dst, bytes + offset, take);
				return take;
			};

			in_progress_message_out.begin_send((uint8_t)MessageType::FieldsRequest, sizeof(uint32_t), writer);
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
			fields_request_receive_state = {};

			auto reader = [this](const uint8_t* data, size_t len)
			{
				size_t cursor = 0;
				while (fields_request_receive_state.tick_rate_bytes_received < sizeof(uint32_t) && cursor < len)
				{
					fields_request_receive_state.tick_rate_bytes[fields_request_receive_state.tick_rate_bytes_received++] = data[cursor++];

					if (fields_request_receive_state.tick_rate_bytes_received == sizeof(uint32_t))
					{
						uint32_t tick_rate_net = 0;
						memcpy(&tick_rate_net, fields_request_receive_state.tick_rate_bytes, sizeof(uint32_t));
						fields_request_receive_state.tick_rate_hz = network_bytes_to_float(tick_rate_net);
					}
				}
			};

			in_progress_message_in.begin_receive(reader);
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
			float received_mutual_tick_rate_hz = fields_request_receive_state.tick_rate_hz;

			if (fields_request_receive_state.tick_rate_bytes_received < sizeof(uint32_t))
			{
				ROBOTICK_WARNING("FieldsRequest missing mutual tick rate payload");
				received_mutual_tick_rate_hz = 0.0f;
			}

			if (robotick::isfinite(received_mutual_tick_rate_hz) && received_mutual_tick_rate_hz > 0.0f)
			{
				ROBOTICK_INFO_IF(
					ROBOTICK_REMOTE_ENGINE_CONNECTION_VERBOSE, "Sender received mutual tick rate: %.2f Hz", received_mutual_tick_rate_hz);
				this->mutual_tick_rate_hz = received_mutual_tick_rate_hz;
			}
			else
			{
				ROBOTICK_WARNING("Invalid mutual tick rate received");
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
			auto writer = [this](size_t offset, uint8_t* dst, size_t max_len) -> size_t
			{
				return write_fields_payload(offset, dst, max_len);
			};

			in_progress_message_out.begin_send((uint8_t)MessageType::Fields, field_payload_capacity, writer);
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
			field_receive_state = {};

			auto reader = [this](const uint8_t* data, size_t len)
			{
				size_t cursor = 0;

				while (cursor < len && field_receive_state.field_index < field_count)
				{
					auto& field = fields[field_receive_state.field_index];
					if (!field.recv_ptr)
					{
						ROBOTICK_FATAL_EXIT("Receiver field '%s' has null recv_ptr", field.path.c_str());
					}

					const size_t remaining_in_field = field.size - field_receive_state.offset_in_field;
					const size_t take = rtk_min(remaining_in_field, len - cursor);

					::memcpy(static_cast<uint8_t*>(field.recv_ptr) + field_receive_state.offset_in_field, data + cursor, take);

					cursor += take;
					field_receive_state.offset_in_field += take;
					field_receive_state.total_bytes_received += take;

					if (field_receive_state.offset_in_field == field.size)
					{
						field_receive_state.field_index++;
						field_receive_state.offset_in_field = 0;
					}
				}

				if (cursor < len)
				{
					ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_receive_fields_as_message() - received more data than expected");
				}
			};

			in_progress_message_in.begin_receive(reader);
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

		// validate sizes
		const size_t expected_bytes = field_payload_capacity;
		const size_t reported_bytes = in_progress_message_in.payload_length();

		if (reported_bytes != expected_bytes)
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_receive_fields_as_message() - payload header reports %zu bytes but expected %zu",
				reported_bytes,
				expected_bytes);
		}

		if (field_receive_state.total_bytes_received != expected_bytes)
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection::tick_receive_fields_as_message() - received %zu bytes but expected %zu",
				field_receive_state.total_bytes_received,
				expected_bytes);
		}

		in_progress_message_in.vacate(); // vacate ready for next user
		return true;
	}

	void RemoteEngineConnection::disconnect()
	{
		// Drain any partially sent/received packets before closing the socket.  This guarantees that both sides either see a
		// fully framed message or a disconnect — never a half-written payload that could desynchronize the protocol.
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

		// Explicit close() even after ConnectionLost lets us reclaim the file descriptor immediately; the next state machine
		// iteration will build a fresh socket with clean errno/flags.
		if (socket_fd >= 0)
			close(socket_fd);
		socket_fd = -1;

		time_sec_to_reconnect = RECONNECT_ATTEMPT_INTERVAL_SEC;

		if (mode == Mode::Receiver)
		{
			// receiver gets told what fields to use by sender, on handshake.  We should therefore clear then whenever we disconnect
			field_count = 0;
			field_payload_capacity = 0;
		}

		set_state(State::Disconnected);
	}

} // namespace robotick
