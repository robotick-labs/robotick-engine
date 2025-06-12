// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace robotick
{

	namespace
	{
		constexpr const char* MAGIC = "RBIN";
		constexpr uint8_t VERSION = 1;

		struct MessageHeader
		{
			char magic[4];
			uint8_t version;
			uint8_t type;
			uint16_t reserved;
			uint32_t payload_len;
		};

		static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be 12 bytes");

		int create_tcp_socket()
		{
			int fd = socket(AF_INET, SOCK_STREAM, 0);
			if (fd < 0)
				ROBOTICK_FATAL_EXIT("Failed to create socket");
			return fd;
		}
	} // namespace

	RemoteEngineConnection::RemoteEngineConnection(const ConnectionConfig& config, Mode mode) : mode(mode), config(config)
	{
	}

	void RemoteEngineConnection::register_field(const Field& field)
	{
		fields.push_back(field);
	}

	void RemoteEngineConnection::set_field_binder(BinderCallback cb)
	{
		binder = std::move(cb);
	}

	bool RemoteEngineConnection::is_connected() const
	{
		return socket_fd >= 0 && state != State::Disconnected;
	}

	bool RemoteEngineConnection::is_ready_for_tick() const
	{
		return state == State::Ticking;
	}

	void RemoteEngineConnection::connect_socket()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Proactive, "RemoteEngineConnection::connect_socket() should only be called in Mode::Proactive");

		socket_fd = create_tcp_socket();
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(config.port);

		if (inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr) != 1)
		{
			close(socket_fd);

			ROBOTICK_FATAL_EXIT("Invalid IP address: %s", config.host.c_str());

			socket_fd = -1;
			return;
		}

		if (connect(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
			ROBOTICK_FATAL_EXIT("Failed to connect to %s", config.host.c_str());

		state = State::Connected;
		ROBOTICK_INFO("Proactive connection established to %s:%d", config.host.c_str(), config.port);
	}

	void RemoteEngineConnection::accept_socket()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Passive, "RemoteEngineConnection::accept_socket() should only be called in Mode::Passive");

		socket_fd = create_tcp_socket();
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(config.port);
		addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
			ROBOTICK_FATAL_EXIT("Failed to bind socket");

		if (listen(socket_fd, 1) < 0)
			ROBOTICK_FATAL_EXIT("Failed to listen on socket");

		sockaddr_in client_addr{};
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(socket_fd, (sockaddr*)&client_addr, &client_len);
		if (client_fd < 0)
			ROBOTICK_FATAL_EXIT("Failed to accept connection");

		close(socket_fd); // Close listener
		socket_fd = client_fd;
		state = State::Connected;

		ROBOTICK_INFO("Passive connection accepted on port %d", config.port);
	}

	void RemoteEngineConnection::send_message(const MessageType type, const uint8_t* data, size_t size)
	{
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Passive && type == MessageType::Subscribe), "Passive connections must not send MessageType::Subscribe");
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Proactive && type == MessageType::Ack), "Proactive connections must not send MessageType::Ack");
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Passive && type == MessageType::Fields), "Passive connections must not send MessageType::Fields");

		MessageHeader header{};
		std::memcpy(header.magic, MAGIC, 4);
		header.version = VERSION;
		header.type = static_cast<uint8_t>(type);
		header.reserved = 0;
		header.payload_len = static_cast<uint32_t>(size);

		// Send header safely
		const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
		size_t header_remaining = sizeof(header);
		while (header_remaining > 0)
		{
			ssize_t sent = send(socket_fd, header_ptr, header_remaining, 0);
			if (sent <= 0)
				return; // Optionally log or handle as a disconnection
			header_ptr += sent;
			header_remaining -= sent;
		}

		// Send payload safely (if any)
		const uint8_t* data_ptr = data;
		size_t data_remaining = size;
		while (data_remaining > 0)
		{
			ssize_t sent = send(socket_fd, data_ptr, data_remaining, 0);
			if (sent <= 0)
				return; // Optionally log or handle as a disconnection
			data_ptr += sent;
			data_remaining -= sent;
		}
	}

	bool RemoteEngineConnection::receive_message(std::vector<uint8_t>& buffer_out)
	{
		// This can be called by Proactive or Passive RemoteEngineConnection during handshake or tick exchange

		MessageHeader header{};
		uint8_t* header_ptr = reinterpret_cast<uint8_t*>(&header);
		size_t header_remaining = sizeof(header);

		// Manually read the full header
		while (header_remaining > 0)
		{
			ssize_t bytes = recv(socket_fd, header_ptr, header_remaining, 0);
			if (bytes <= 0)
				return false;
			header_ptr += bytes;
			header_remaining -= bytes;
		}

		if (std::memcmp(header.magic, MAGIC, 4) != 0 || header.version != VERSION)
			return false;

		// An attacker could send a message with a huge payload_len causing memory exhaustion. Add a maximum payload size limit.
		constexpr size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
		if (header.payload_len > MAX_PAYLOAD_SIZE)
		{
			ROBOTICK_WARNING("Payload too large: %u", header.payload_len);
			return false;
		}

		// Now read the payload
		if (header.payload_len > 0)
		{
			buffer_out.resize(header.payload_len);
			uint8_t* payload_ptr = buffer_out.data();
			size_t payload_remaining = header.payload_len;

			while (payload_remaining > 0)
			{
				ssize_t bytes = recv(socket_fd, payload_ptr, payload_remaining, 0);
				if (bytes <= 0)
					return false;
				payload_ptr += bytes;
				payload_remaining -= bytes;
			}
		}
		else
		{
			buffer_out.clear();
		}

		return true;
	}

	void RemoteEngineConnection::send_fields_as_message()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Proactive, "RemoteEngineConnection::send_fields_as_message() should only be called in Mode::Proactive");

		std::vector<uint8_t> buffer;
		for (const auto& field : fields)
		{
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(field.send_ptr);
			if (!ptr)
				continue;
			buffer.insert(buffer.end(), ptr, ptr + field.size);
		}
		send_message(MessageType::Fields, buffer.data(), buffer.size());
	}

	void RemoteEngineConnection::receive_into_fields()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Passive, "RemoteEngineConnection::receive_into_fields() should only be called in Mode::Passive");

		std::vector<uint8_t> buffer;
		if (!receive_message(buffer))
			return;

		size_t offset = 0;
		for (auto& field : fields)
		{
			if (offset + field.size > buffer.size())
			{
				ROBOTICK_FATAL_EXIT("RemoteEngineConnection::receive_into_fields() - buffer received is too small for all expected fields");
				break;
			}

			std::memcpy(field.recv_ptr, buffer.data() + offset, field.size);
			offset += field.size;
		}
	}

	void RemoteEngineConnection::send_handshake()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Proactive, "RemoteEngineConnection::send_handshake() should only be called in Mode::Proactive");

		std::vector<uint8_t> payload;

		bool is_first_field = true;

		for (const auto& field : fields)
		{
			if (!is_first_field)
			{
				payload.push_back('\n');
			}

			payload.insert(payload.end(), field.path.begin(), field.path.end());

			is_first_field = false;
		}

		send_message(MessageType::Subscribe, payload.data(), payload.size());

		ROBOTICK_INFO("Proactive handshake sent with %zu field(s)", fields.size());
	}

	void RemoteEngineConnection::receive_handshake_and_bind()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Passive, "RemoteEngineConnection::receive_handshake_and_bind() should only be called in Mode::Passive");

		std::vector<uint8_t> buffer;
		if (!receive_message(buffer))
			return;

		// receive info
		std::string data(reinterpret_cast<char*>(buffer.data()), buffer.size());
		size_t start = 0;
		int bound_count = 0;
		for (size_t index = 0; index <= data.size(); ++index)
		{
			if (index == data.size() || data[index] == '\n')
			{
				std::string path = data.substr(start, index - start);
				start = index + 1;

				Field field;
				if (!binder || !binder(path, field))
				{
					ROBOTICK_WARNING("Failed to bind field: %s", path.c_str());
					continue;
				}
				fields.push_back(field);
				bound_count++;
			}
		}

		send_message(MessageType::Ack, nullptr, 0);
		ROBOTICK_INFO("Passive handshake received. Bound %d field(s)", bound_count);
	}

	void RemoteEngineConnection::handle_handshake()
	{
		if (mode == Mode::Proactive)
		{
			send_handshake();
			std::vector<uint8_t> ack;
			if (receive_message(ack))
			{
				state = State::Subscribed;
				ROBOTICK_INFO("Proactive connection received handshake ACK");
			}
		}
		else
		{
			receive_handshake_and_bind();
			state = State::Subscribed;
		}
	}

	void RemoteEngineConnection::handle_tick_exchange()
	{
		if (mode == Mode::Proactive)
		{
			send_fields_as_message();
		}
		else
		{
			receive_into_fields();
		}

		if (state != State::Ticking)
		{
			state = State::Ticking;
			ROBOTICK_INFO("RemoteEngineConnection [%s] is now ticking", mode == Mode::Proactive ? "Mode::Proactive" : "Mode::Passive");
		}
	}

	void RemoteEngineConnection::tick()
	{
		if (!is_connected())
		{
			if (mode == Mode::Proactive)
				connect_socket();
			else
				accept_socket();
		}
		if (state == State::Connected)
			handle_handshake();
		if (state == State::Subscribed)
			handle_tick_exchange();
	}

	void RemoteEngineConnection::cleanup()
	{
		if (socket_fd >= 0)
			close(socket_fd);
		socket_fd = -1;
		state = State::Disconnected;
	}

} // namespace robotick
