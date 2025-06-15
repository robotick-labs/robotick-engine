
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <arpa/inet.h>
#include <csignal> // For signal(), SIGPIPE, SIG_IGN
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define ROBOTICK_REMOTE_ENGINE_LOG_PACKETS 0

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

			int opt = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
				ROBOTICK_WARNING("Failed to set SO_REUSEADDR on socket");

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

		const char* mode_str = (mode == Mode::Receiver) ? "Receiver" : "Sender";

		if (state == State::Disconnected)
		{
			ROBOTICK_INFO("RemoteEngineConnection [%s] - disconnected", mode_str);
		}
		else if (state == State::ReadyForHandshake)
		{
			ROBOTICK_INFO("RemoteEngineConnection [%s] - socket-connection established, ready for handshake", mode_str);
		}
		else if (state == State::ReadyForHandshakeAck)
		{
			ROBOTICK_INFO("RemoteEngineConnection [%s] - handshake sent - awaiting acknowledgement from remote-listener", mode_str);
		}
		else if (state == State::Ready)
		{
			const char* field_data_str = (mode == Mode::Receiver) ? "receive" : "send";
			ROBOTICK_INFO("RemoteEngineConnection [%s] - ready to %s field data!", mode_str, field_data_str);
		}
		else
		{
			ROBOTICK_FATAL_EXIT("RemoteEngineConnection [%s]- unknown state %i", mode_str, (int)state);
		}
	}

	void ensure_sigpipe_ignored()
	{
		signal(SIGPIPE, SIG_IGN);
	}

	// configure() should be a called on all connections first - establishing our operation mode (sender / receiver) and remote-connection info
	void RemoteEngineConnection::configure(const ConnectionConfig& config_in, Mode mode_in)
	{
		ensure_sigpipe_ignored(); // ensure we don't trigger an exception if connection is lost mid-send - we know to handle -1 anyway

		config = config_in;
		mode = mode_in;
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

	void RemoteEngineConnection::tick()
	{
		if (!has_basic_connection())
		{
			if (mode == Mode::Sender)
				connect_socket();
			else
				accept_socket();

			if (state == State::Disconnected)
			{
				return;
			}
			// else have an initial go at the below
		}
		if (state == State::ReadyForHandshake)
			handle_handshake();
		if (state == State::ReadyForHandshakeAck)
			try_receive_handshake_ack();
		if (state == State::Ready)
			handle_tick_exchange();
	}

	bool RemoteEngineConnection::has_basic_connection() const
	{
		return socket_fd >= 0 && state != State::Disconnected;
	}

	bool RemoteEngineConnection::is_ready() const
	{
		return state == State::Ready;
	}

	void RemoteEngineConnection::connect_socket()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::connect_socket() should only be called in Mode::Sender");

		socket_fd = create_tcp_socket();
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(config.port);

		if (inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr) != 1)
		{
			close(socket_fd);
			socket_fd = -1;
			ROBOTICK_WARNING("Invalid IP address: %s", config.host.c_str());
			return;
		}

		if (connect(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
		{
			if (errno != EINPROGRESS)
			{
				close(socket_fd);
				socket_fd = -1;
				ROBOTICK_WARNING("Failed to connect to %s:%d", config.host.c_str(), config.port);
				return;
			}
		}

		ROBOTICK_INFO("Sender connection initiated to %s:%d", config.host.c_str(), config.port);

		set_state(State::ReadyForHandshake);
	}

	void RemoteEngineConnection::accept_socket()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::accept_socket() should only be called in Mode::Receiver");

		if (socket_fd < 0)
		{
			socket_fd = create_tcp_socket();
			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(config.port);
			addr.sin_addr.s_addr = INADDR_ANY;

			if (bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
				ROBOTICK_FATAL_EXIT("Failed to bind socket");

			if (listen(socket_fd, 1) < 0)
				ROBOTICK_FATAL_EXIT("Failed to listen on socket");

			return;
		}

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
		ROBOTICK_INFO("Receiver connection accepted on port %d", config.port);

		set_state(State::ReadyForHandshake);
	}

	void RemoteEngineConnection::send_handshake()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::send_handshake() should only be called in Mode::Sender");

		if (fields.size() == 0)
		{
			ROBOTICK_FATAL_EXIT(
				"RemoteEngineConnection::send_handshake() being called with no prior call(s) to RemoteEngineConnection::register_field()");
		}

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

		const SendResult send_result = send_message(MessageType::Subscribe, payload.data(), payload.size());
		if (send_result != SendResult::Success)
		{
			if (send_result == SendResult::ConnectionLost)
			{
				disconnect();
			}

			return;
		}

		ROBOTICK_INFO("Sender handshake sent with %zu field(s)", fields.size());
		set_state(State::ReadyForHandshakeAck);
	}

	void RemoteEngineConnection::receive_handshake_and_bind()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::receive_handshake_and_bind() should only be called in Mode::Receiver");

		std::vector<uint8_t> buffer;
		const ReceiveResult receive_result = receive_message(buffer);
		if (receive_result != ReceiveResult::MessageReceived)
		{
			if (receive_result == ReceiveResult::ConnectionLost)
			{
				disconnect(); // try to regain connection next tick
			}

			return;
		}

		std ::string data(reinterpret_cast<char*>(buffer.data()), buffer.size());
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

		const SendResult send_result = send_message(MessageType::Ack, nullptr, 0);
		if (send_result != SendResult::Success)
		{
			if (send_result == SendResult::ConnectionLost)
			{
				disconnect();
			}

			return;
		}

		ROBOTICK_INFO("Receiver handshake received. Bound %d field(s)", bound_count);
		set_state(State::Ready);
	}

	void RemoteEngineConnection::handle_handshake()
	{
		if (mode == Mode::Sender)
		{
			send_handshake();
		}
		else
		{
			receive_handshake_and_bind();
		}
	}

	void RemoteEngineConnection::try_receive_handshake_ack()
	{
		std::vector<uint8_t> ack;

		const ReceiveResult receive_result = receive_message(ack);
		if (receive_result != ReceiveResult::MessageReceived)
		{
			if (receive_result == ReceiveResult::ConnectionLost)
			{
				disconnect(); // try to regain connection next tick
			}

			return;
		}

		set_state(State::Ready);
	}

#if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS
	void log_send_preview(
		const char* label, const RemoteEngineConnection::MessageType type, const uint8_t* data, size_t size, size_t max_preview = 64)
	{
		std::string preview;
		size_t count = std::min(size, max_preview);
		for (size_t i = 0; i < count; ++i)
		{
			char c = static_cast<char>(data[i]);
			preview += (c != 0) ? c : '.';
		}
		if (size > max_preview)
			preview += "...";

		ROBOTICK_INFO("%s [%d] (%zu bytes): '%s'", label, (int)type, size, preview.c_str());
	}

#else
	inline void log_send_preview(const char*, const RemoteEngineConnection::MessageType, const uint8_t*, size_t, size_t = 64) {};
#endif // #if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS

	RemoteEngineConnection::SendResult RemoteEngineConnection::send_message(const MessageType type, const uint8_t* data, size_t size)
	{
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Receiver && type == MessageType::Subscribe), "Receiver connections must not send MessageType::Subscribe");
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Sender && type == MessageType::Ack), "Sender connections must not send MessageType::Ack");
		ROBOTICK_ASSERT_MSG(!(mode == Mode::Receiver && type == MessageType::Fields), "Receiver connections must not send MessageType::Fields");

		MessageHeader header{};
		std::memcpy(header.magic, MAGIC, 4);
		header.version = VERSION;
		header.type = static_cast<uint8_t>(type);
		header.reserved = 0;
		header.payload_len = static_cast<uint32_t>(size);

		const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
		size_t header_remaining = sizeof(header);
		while (header_remaining > 0)
		{
			log_send_preview("Sending header", type, reinterpret_cast<const uint8_t*>(&header), sizeof(header));

			ssize_t sent = send(socket_fd, header_ptr, header_remaining, MSG_NOSIGNAL);
			if (sent <= 0)
			{
				if (sent == 0)
				{
					ROBOTICK_WARNING("send_message(): header send returned 0 bytes — unexpected connection stall");
				}
				else
				{
					ROBOTICK_WARNING("send_message(): failed to send header (errno=%d: %s)", errno, strerror(errno));
				}
				return SendResult::ConnectionLost;
			}
			header_ptr += sent;
			header_remaining -= sent;
		}

		const uint8_t* data_ptr = data;
		size_t data_remaining = size;
		while (data_remaining > 0)
		{
			log_send_preview("Sending payload", type, data, size);

			ssize_t sent = send(socket_fd, data_ptr, data_remaining, MSG_NOSIGNAL);
			if (sent <= 0)
			{
				if (sent == 0)
				{
					ROBOTICK_WARNING("send_message(): payload send returned 0 bytes — unexpected connection stall");
				}
				else
				{
					ROBOTICK_WARNING("send_message(): failed to send payload (errno=%d: %s)", errno, strerror(errno));
				}
				return SendResult::ConnectionLost;
			}
			data_ptr += sent;
			data_remaining -= sent;
		}

		return SendResult::Success;
	}

#if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS
	void log_recv_preview(const char* label, const RemoteEngineConnection::MessageType type, const uint8_t* data, size_t size, size_t payload_len,
		size_t max_preview = 64)
	{
		std::string preview;
		size_t count = std::min(size, max_preview);
		for (size_t i = 0; i < count; ++i)
		{
			char c = static_cast<char>(data[i]);
			preview += (c != 0) ? c : '.';
		}
		if (size > max_preview)
			preview += "...";

		ROBOTICK_INFO("%s [%d] (%zu bytes) (payload %zu bytes): '%s'", label, (int)type, size, payload_len, preview.c_str());
	}
#else
	inline void log_recv_preview(const char*, const RemoteEngineConnection::MessageType, const uint8_t*, size_t, size_t, size_t = 64) {};
#endif // #if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS

	RemoteEngineConnection::ReceiveResult RemoteEngineConnection::receive_message(std::vector<uint8_t>& buffer_out)
	{
		MessageHeader header{};
		uint8_t* header_ptr = reinterpret_cast<uint8_t*>(&header);
		size_t header_remaining = sizeof(header);

		while (header_remaining > 0)
		{
			ssize_t bytes = recv(socket_fd, header_ptr, header_remaining, 0);
			if (bytes < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					return ReceiveResult::NoMessageYet;

				ROBOTICK_WARNING("receive_message(): failed to receive header (errno=%d: %s)", errno, strerror(errno));
				return ReceiveResult::ConnectionLost;
			}
			if (bytes == 0)
			{
				ROBOTICK_WARNING("receive_message(): connection closed by peer during header read");
				return ReceiveResult::ConnectionLost;
			}
			header_ptr += bytes;
			header_remaining -= bytes;
		}

		log_recv_preview("Received header", (MessageType)header.type, reinterpret_cast<const uint8_t*>(&header), sizeof(header), header.payload_len);

		if (std::memcmp(header.magic, MAGIC, 4) != 0 || header.version != VERSION)
		{
			ROBOTICK_WARNING(
				"receive_message(): header validation failed (bad magic or version). Got magic=%.4s version=%d", header.magic, header.version);
			return ReceiveResult::ConnectionLost;
		}

		constexpr size_t MAX_PAYLOAD_SIZE = 1024 * 1024;
		if (header.payload_len > MAX_PAYLOAD_SIZE)
		{
			ROBOTICK_WARNING("receive_message(): payload too large (%u bytes, max allowed %zu)", header.payload_len, MAX_PAYLOAD_SIZE);
			return ReceiveResult::ConnectionLost;
		}

		if (header.payload_len > 0)
		{
			buffer_out.resize(header.payload_len);
			uint8_t* payload_ptr = buffer_out.data();
			size_t payload_remaining = header.payload_len;

			while (payload_remaining > 0)
			{
				ssize_t bytes = recv(socket_fd, payload_ptr, payload_remaining, 0);
				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						// We must wait until the entire payload is received to avoid desynchronizing the stream.
						// Returning early here would cause the next call to interpret mid-payload data as a new header.
						// If this proves a performance bottleneck, we could split the state into ReadyForHeader / ReadyForPayload
						// to support true non-blocking operation without risking misalignment.

						robotick::Thread::sleep_ms(1);
						continue;
					}

					ROBOTICK_WARNING("receive_message(): failed to receive payload (errno=%d: %s)", errno, strerror(errno));
					return ReceiveResult::ConnectionLost;
				}
				if (bytes == 0)
				{
					ROBOTICK_WARNING("receive_message(): connection closed by peer during payload read");
					return ReceiveResult::ConnectionLost;
				}
				payload_ptr += bytes;
				payload_remaining -= bytes;
			}

			log_recv_preview("Received payload", (MessageType)header.type, buffer_out.data(), buffer_out.size(), 0);
		}
		else
		{
			buffer_out.clear();
		}

		return ReceiveResult::MessageReceived;
	}

	void RemoteEngineConnection::send_fields_as_message()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Sender, "RemoteEngineConnection::send_fields_as_message() should only be called in Mode::Sender");

		std::vector<uint8_t> buffer;
		for (const auto& field : fields)
		{
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(field.send_ptr);
			if (!ptr)
				continue;
			buffer.insert(buffer.end(), ptr, ptr + field.size);
		}

		const SendResult send_result = send_message(MessageType::Fields, buffer.data(), buffer.size());
		if (send_result != SendResult::Success)
		{
			if (send_result == SendResult::ConnectionLost)
			{
				disconnect();
			}

			return;
		}
	}

	void RemoteEngineConnection::receive_into_fields()
	{
		ROBOTICK_ASSERT_MSG(mode == Mode::Receiver, "RemoteEngineConnection::receive_into_fields() should only be called in Mode::Receiver");

		std::vector<uint8_t> buffer;

		const ReceiveResult receive_result = receive_message(buffer);
		if (receive_result != ReceiveResult::MessageReceived)
		{
			if (receive_result == ReceiveResult::ConnectionLost)
			{
				disconnect(); // try to regain connection next tick
			}

			return;
		}

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

	void RemoteEngineConnection::handle_tick_exchange()
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
		if (socket_fd >= 0)
			close(socket_fd);
		socket_fd = -1;

		set_state(State::Disconnected);
	}

} // namespace robotick
