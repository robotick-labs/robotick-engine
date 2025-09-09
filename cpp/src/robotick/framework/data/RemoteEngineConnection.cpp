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

		const auto prev_state = state;
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
		else if (state == State::ReadyForFieldsRequest)
		{
			const bool show_always = false;
			if (show_always || prev_state != State::ReadyForFields) // only show after connecting, to minimise spam
			{
				const char* field_data_str = is_receiver ? "send" : "receive";
				ROBOTICK_INFO(
					"%s[%s] [-> State::ReadyForFieldsRequest] - ready to %s fields-request!%s", color_start, mode_str, field_data_str, color_end);
			}
		}
		else if (state == State::ReadyForFields)
		{
			const bool show_always = false;
			if (show_always)
			{
				const char* field_data_str = is_receiver ? "receive" : "send";
				ROBOTICK_INFO("%s[%s] [-> State::ReadyForFields] - ready to %s fields-data!%s", color_start, mode_str, field_data_str, color_end);
			}
		}
		else
		{
			ROBOTICK_FATAL_EXIT("[%s] - unknown state %i", mode_str, static_cast<int>(state));
		}
	}

	void RemoteEngineConnection::configure_sender(const char* my_name, const char* target_name)
	{
		mode = Mode::Sender;
		my_model_name = my_name;
		target_model_name = target_name;
		resolved_host = "";
		resolved_port = -1;

		discovery.set_on_discovered(
			[this](const UDPDiscoveryManager::PeerInfo& info)
			{
				if (info.model_id == target_model_name)
				{
					resolved_host = info.ip;
					resolved_port = info.port;

					ROBOTICK_INFO("Discovered peer [%s] at %s:%d", info.model_id.c_str(), info.ip.c_str(), info.port);
				}
				else
				{
					ROBOTICK_WARNING("Discovered unexpected peer [%s] at %s:%d", info.model_id.c_str(), info.ip.c_str(), info.port);
				}
			});

		discovery.initialize(my_model_name.c_str(), 0, DiscoveryMode::Sender);

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
		discovery.tick();
		ROBOTICK_INFO("HERE 1");

		if (state == State::Disconnected)
		{
			ROBOTICK_INFO("HERE 2 - %f", time_sec_to_reconnect);
			if (time_sec_to_reconnect > 0.0F)
			{
				// wait a bit longer before trying to reconnect (try every RECONNECT_ATTEMPT_INTERVAL_SEC)
				time_sec_to_reconnect -= (float)tick_info.delta_time;
				return;
			}

			ROBOTICK_INFO("HERE 3");

			if (mode == Mode::Sender)
			{
				tick_disconnected_sender(tick_info);
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
			tick_ready_for_handshake();
		}
		if (state == State::ReadyForFieldsRequest)
		{
			tick_ready_for_field_request();
		}
		if (state == State::ReadyForFields)
		{
			tick_ready_for_fields();
		}
	}

	bool RemoteEngineConnection::has_basic_connection() const
	{
		return socket_fd >= 0 && state != State::Disconnected;
	}

	bool RemoteEngineConnection::is_ready() const
	{
		return state == State::ReadyForFieldsRequest || state == State::ReadyForFields;
	}

	void RemoteEngineConnection::tick_disconnected_sender(const TickInfo& tick_info)
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_disconnected_sender() should only be called in Mode::Sender");

		// Broadcast DISCOVER_PEER periodically until resolved
		static constexpr float BROADCAST_INTERVAL_SEC = 0.1f;
		static float time_since_last_broadcast = 0.0f;

		ROBOTICK_INFO("HERE A");

		time_since_last_broadcast += tick_info.delta_time;

		if (resolved_port < 0)
		{
			ROBOTICK_INFO("HERE B");

			if (time_since_last_broadcast >= BROADCAST_INTERVAL_SEC)
			{
				ROBOTICK_INFO("HERE B");
				discovery.broadcast_discovery_request(target_model_name.c_str());
				time_since_last_broadcast = 0.0f;
			}

			// Wait until our on_discovered callback fires
			return;
		}

		socket_fd = create_tcp_socket();
		if (socket_fd < 0)
		{
			return;
		}

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(resolved_port);

		if (inet_pton(AF_INET, resolved_host.c_str(), &addr.sin_addr) != 1)
		{
			ROBOTICK_WARNING("Invalid IP address: %s", resolved_host.c_str());
			close(socket_fd);
			socket_fd = -1;
			resolved_port = -1;
			return;
		}

		if (connect(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
		{
			if (errno != EINPROGRESS)
			{
				ROBOTICK_WARNING("Failed to connect to %s:%d", resolved_host.c_str(), resolved_port);
				close(socket_fd);
				socket_fd = -1;
				resolved_port = -1; // Reset so we retry discovery
				return;
			}
		}

		ROBOTICK_INFO("Sender [%s] initiated connection to [%s] @ %s:%d",
			my_model_name.c_str(),
			target_model_name.c_str(),
			resolved_host.c_str(),
			resolved_port);

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
			addr.sin_port = htons(0); // 👈 ask OS for ephemeral port
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

				discovery.initialize(my_model_name.c_str(), listen_port, DiscoveryMode::Receiver);
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

	void RemoteEngineConnection::tick_send_handshake()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::tick_send_handshake() should only be called in Mode::Sender");

		if (fields.size() == 0)
		{
			ROBOTICK_FATAL_EXIT(
				"RemoteEngineConnection::tick_send_handshake() being called with no prior call(s) to RemoteEngineConnection::register_field()");
		}

		if (in_progress_message.is_vacant())
		{
			std::vector<uint8_t> payload;
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

			in_progress_message.begin_send((uint8_t)MessageType::Subscribe, payload.data(), payload.size());
		}

		const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost sending handshake from Sender");
			disconnect();
			return;
		}

		if (in_progress_message.is_completed())
		{
			in_progress_message.vacate(); // vacate ready for next user

			ROBOTICK_INFO("Sender handshake sent with %zu field(s)", fields.size());
			set_state(State::ReadyForFieldsRequest);
		}
	}

	void RemoteEngineConnection::tick_receive_handshake_and_bind()
	{
		ROBOTICK_ASSERT_MSG(
			mode == Mode::Receiver, "RemoteEngineConnection::tick_receive_handshake_and_bind() should only be called in Mode::Receiver");

		if (!binder)
		{
			ROBOTICK_FATAL_EXIT("Receiver connection has no binder callback set before handshake");
		}

		if (in_progress_message.is_vacant())
		{
			in_progress_message.begin_receive();
		}

		const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost receiving handshake from Sender");
			disconnect();
			return;
		}

		if (in_progress_message.is_completed())
		{
			auto [payload_data, payload_size] = in_progress_message.get_payload();

			std::string data(reinterpret_cast<const char*>(payload_data), payload_size);
			size_t start = 0;
			size_t bound_count = 0;
			size_t failed_count = 0;

			for (size_t index = 0; index <= data.size(); ++index)
			{
				if (index == data.size() || data[index] == '\n')
				{
					std::string path = data.substr(start, index - start);
					start = index + 1;

					Field field;

					if (!binder(path.c_str(), field))
					{
						ROBOTICK_WARNING("Failed to bind field: %s", path.c_str());
						failed_count++;
						continue;
					}
					fields.push_back(field);
					bound_count++;
				}
			}

			if (failed_count > 0)
			{
				ROBOTICK_FATAL_EXIT("Failed to bind %zu fields - disconnecting", failed_count);
			}

			in_progress_message.vacate(); // vacate ready for next user

			ROBOTICK_INFO("Receiver handshake received. Bound %zu field(s) - total %zu (should be same value)", bound_count, fields.size());
			set_state(State::ReadyForFieldsRequest);
		}
	}

	void RemoteEngineConnection::tick_ready_for_handshake()
	{
		if (mode == Mode::Sender)
		{
			tick_send_handshake();
		}
		else
		{
			tick_receive_handshake_and_bind();
		}
	}

	void RemoteEngineConnection::tick_ready_for_field_request()
	{
		std::vector<uint8_t> ack;

		if (mode == Mode::Sender)
		{
			if (in_progress_message.is_vacant())
			{
				in_progress_message.begin_receive();
			}

			const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
			if (tick_result == InProgressMessage::Result::ConnectionLost)
			{
				ROBOTICK_WARNING("Connection lost receiving field-request from Receiver");
				disconnect();
				return;
			}

			if (in_progress_message.is_completed())
			{
				in_progress_message.vacate(); // vacate ready for next user

				set_state(State::ReadyForFields);
			}
		}
		else
		{
			if (in_progress_message.is_vacant())
			{
				in_progress_message.begin_send((uint8_t)MessageType::FieldsRequest, nullptr, 0);
			}

			const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
			if (tick_result == InProgressMessage::Result::ConnectionLost)
			{
				ROBOTICK_WARNING("Connection lost sending field-request from Receiver");
				disconnect();
				return;
			}

			if (in_progress_message.is_completed())
			{
				in_progress_message.vacate(); // vacate ready for next user

				set_state(State::ReadyForFields);
			}
		}
	}

	void RemoteEngineConnection::send_fields_as_message()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::send_fields_as_message() should only be called in Mode::Sender");

		if (in_progress_message.is_vacant())
		{
			std::vector<uint8_t> buffer;
			for (const auto& field : fields)
			{
				const uint8_t* ptr = reinterpret_cast<const uint8_t*>(field.send_ptr);
				if (!ptr)
					continue;
				buffer.insert(buffer.end(), ptr, ptr + field.size);
			}

			in_progress_message.begin_send((uint8_t)MessageType::Fields, buffer.data(), buffer.size());
		}

		const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost sending field-data from Sender");
			disconnect();
			return;
		}

		if (in_progress_message.is_completed())
		{
			in_progress_message.vacate(); // vacate ready for next user

			set_state(State::ReadyForFieldsRequest);
		}
	}

	void RemoteEngineConnection::receive_into_fields()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::receive_into_fields() should only be called in Mode::Receiver");

		if (in_progress_message.is_vacant())
		{
			in_progress_message.begin_receive();
		}

		const InProgressMessage::Result tick_result = in_progress_message.tick(socket_fd);
		if (tick_result == InProgressMessage::Result::ConnectionLost)
		{
			ROBOTICK_WARNING("Connection lost receiving field-data from Sender");
			disconnect();
			return;
		}

		if (in_progress_message.is_completed())
		{
			auto [payload_data, payload_size] = in_progress_message.get_payload();

			size_t offset_into_payload = 0;
			for (auto& field : fields)
			{
				if (offset_into_payload + field.size > payload_size)
				{
					ROBOTICK_FATAL_EXIT(
						"RemoteEngineConnection::receive_into_fields() - buffer received is too small (%zu bytes) for all expected fields (%zu)",
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

			in_progress_message.vacate(); // vacate ready for next user

			set_state(State::ReadyForFieldsRequest);
		}
	}

	void RemoteEngineConnection::tick_ready_for_fields()
	{
		if (mode == Mode::Sender)
		{
			send_fields_as_message();
		}
		else
		{
			receive_into_fields();
		}
	}

	void RemoteEngineConnection::disconnect()
	{
		// tick in_progress_message if occupied, to ensure we're not sending partial messages
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

		if (socket_fd >= 0)
			close(socket_fd);
		socket_fd = -1;

		time_sec_to_reconnect = RECONNECT_ATTEMPT_INTERVAL_SEC;

		if (mode == Mode::Receiver)
		{
			// receiver gets told what fields to use by sender, on handshake.  We should therefore clear then whenever we disconnect
			fields.clear();
		}

		if (mode == Mode::Sender)
		{
			resolved_host.clear();
			resolved_port = -1;
		}

		set_state(State::Disconnected);
	}

} // namespace robotick
