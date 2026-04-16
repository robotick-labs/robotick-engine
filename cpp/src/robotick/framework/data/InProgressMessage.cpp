// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/InProgressMessage.h"

#include "robotick/api.h"
#include "robotick/framework/data/MessageHeader.h"

#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#define ROBOTICK_REMOTE_ENGINE_LOG_PACKETS 0

namespace robotick
{
	namespace
	{
		inline size_t min_size(size_t a, size_t b)
		{
			return (a < b) ? a : b;
		}
	} // namespace

#if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS
	static void log_preview(const char* label, const MessageHeader& header, const uint8_t* data, size_t size)
	{
		char preview[68] = {};
		const size_t max_preview = 64;
		size_t count = (data != nullptr) ? min_size(size, max_preview) : 0;
		for (size_t i = 0; i < count; ++i)
		{
			char c = static_cast<char>(data[i]);
			preview[i] = (c != 0) ? c : '.';
		}
		size_t preview_len = count;
		if (size > max_preview && preview_len + 3 < sizeof(preview))
		{
			preview[preview_len++] = '.';
			preview[preview_len++] = '.';
			preview[preview_len++] = '.';
		}
		preview[preview_len] = '\0';

		ROBOTICK_INFO("%s [%d] (%zu bytes, payload %zu bytes): '%s'", label, header.type, size, (size_t)header.payload_len, preview);
	}
#else
	static void log_preview(const char*, const MessageHeader&, const uint8_t*, size_t)
	{
	}
#endif

	void InProgressMessage::reserve_payload_capacity(size_t payload_capacity)
	{
		ROBOTICK_ASSERT_MSG(!(stage == Stage::Sending && payload_bytes_sent > 0),
			"reserve_payload_capacity() must not grow the send buffer after payload transmission has started");
		ROBOTICK_ASSERT_MSG(!(stage == Stage::Receiving && payload_bytes_received > 0),
			"reserve_payload_capacity() must not grow the receive buffer after payload reception has started");

		if (payload_capacity <= payload_buffer_capacity)
		{
			return;
		}

		uint8_t* new_payload_buffer = new uint8_t[payload_capacity];
		const size_t bytes_to_preserve = (stage == Stage::Sending) ? payload_size : 0;
		if (payload_buffer != nullptr && bytes_to_preserve > 0)
		{
			::memcpy(new_payload_buffer, payload_buffer, bytes_to_preserve);
		}
		delete[] payload_buffer;
		payload_buffer = new_payload_buffer;
		payload_buffer_capacity = payload_capacity;
	}

	void InProgressMessage::begin_send(uint8_t message_type, size_t payload_size_in, const PayloadWriter& writer)
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_send() should only ever be called when vacant");
		ROBOTICK_ASSERT_MSG(payload_size_in <= kMaxPayloadBytes, "Message payload too large (%zu bytes)", payload_size_in);
		if (payload_size_in > 0)
		{
			reserve_payload_capacity(payload_size_in);
		}

		stage = Stage::Sending;
		header = {};

		static_assert(sizeof(InProgressMessage::kMagic) == sizeof(header.magic));

		::memcpy(header.magic, InProgressMessage::kMagic, sizeof(InProgressMessage::kMagic));
		header.version = InProgressMessage::kVersion;
		header.type = message_type;
		header.payload_len = static_cast<uint32_t>(payload_size_in);

		header_bytes_sent = 0;
		payload_size = payload_size_in;
		payload_bytes_sent = 0;

		if (payload_size_in > 0)
		{
			size_t payload_offset = 0;
			while (payload_offset < payload_size_in)
			{
				const size_t bytes_written = writer ? writer(payload_offset, payload_buffer + payload_offset, payload_size_in - payload_offset) : 0;
				if (bytes_written == 0)
				{
					ROBOTICK_FATAL_EXIT("Payload writer returned zero bytes while %zu bytes remain", payload_size_in - payload_offset);
				}
				payload_offset += bytes_written;
			}
		}
	}

	void InProgressMessage::begin_receive(const PayloadReader& reader, ReceiveMode mode)
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_receive() should only ever be called when vacant");

		stage = Stage::Receiving;
		header = {};
		header_bytes_received = 0;
		payload_bytes_received = 0;
		payload_reader = reader;
		receive_mode = mode;
	}

	void InProgressMessage::vacate()
	{
		stage = Stage::Vacant;
		header = {};
		header_bytes_sent = 0;
		payload_size = 0;
		payload_bytes_sent = 0;
		header_bytes_received = 0;
		payload_bytes_received = 0;
		payload_reader = nullptr;
		receive_mode = ReceiveMode::StagePayload;
	}

	// Called repeatedly from RemoteEngineConnection::tick() so sockets can make forward progress without blocking.
	// Returning InProgress keeps the state machine live, while ConnectionLost tells the owner to tear the socket down.
	InProgressMessage::Result InProgressMessage::tick(int socket_fd)
	{
		if (stage == Stage::Vacant)
			return Result::Completed;

		// ---------------------------
		// Sending path
		// ---------------------------
		if (stage == Stage::Sending)
		{
			while (header_bytes_sent < sizeof(MessageHeader))
			{
				uint8_t header_bytes[sizeof(MessageHeader)];
				header.serialize(header_bytes);

				const size_t remaining = sizeof(MessageHeader) - header_bytes_sent;
				ssize_t bytes = send(socket_fd, header_bytes + header_bytes_sent, remaining, MSG_NOSIGNAL);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::InProgress;

				header_bytes_sent += static_cast<size_t>(bytes);
			}

			while (payload_bytes_sent < payload_size)
			{
				const size_t remaining = payload_size - payload_bytes_sent;
				ssize_t bytes = send(socket_fd, payload_buffer + payload_bytes_sent, remaining, MSG_NOSIGNAL);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::InProgress;

				payload_bytes_sent += static_cast<size_t>(bytes);
			}

			log_preview("Sent full message", header, nullptr, payload_size);
			stage = Stage::Completed;
			return Result::Completed;
		}

		// ---------------------------
		// Receiving path
		// ---------------------------
		if (stage == Stage::Receiving)
		{
			while (header_bytes_received < sizeof(MessageHeader))
			{
				const size_t remaining = sizeof(MessageHeader) - header_bytes_received;
				uint8_t* header_bytes = reinterpret_cast<uint8_t*>(&header);
				ssize_t bytes = recv(socket_fd, header_bytes + header_bytes_received, remaining, 0);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::ConnectionLost;

				header_bytes_received += static_cast<size_t>(bytes);
				if (header_bytes_received < sizeof(MessageHeader))
				{
					continue;
				}

				header.deserialize(header_bytes);

				static_assert(sizeof(InProgressMessage::kMagic) == sizeof(header.magic));

				if (memcmp(header.magic, InProgressMessage::kMagic, sizeof(InProgressMessage::kMagic)) != 0 ||
					header.version != InProgressMessage::kVersion)
				{
					ROBOTICK_WARNING("InProgressMessage::tick(): Invalid header magic or version");
					return Result::ConnectionLost;
				}

				if (header.payload_len > kMaxPayloadBytes)
				{
					ROBOTICK_WARNING("InProgressMessage::tick(): Payload too large (%u bytes)", (unsigned int)header.payload_len);
					return Result::ConnectionLost;
				}

				if (receive_mode == ReceiveMode::StagePayload && header.payload_len > 0)
				{
					reserve_payload_capacity(header.payload_len);
				}
			}

			while (payload_bytes_received < header.payload_len)
			{
				const size_t remaining = header.payload_len - payload_bytes_received;
				uint8_t streamed_payload[1024];
				uint8_t* recv_dst = nullptr;
				size_t recv_len = 0;
				if (receive_mode == ReceiveMode::StreamPayload)
				{
					recv_dst = streamed_payload;
					recv_len = min_size(remaining, sizeof(streamed_payload));
				}
				else
				{
					recv_dst = payload_buffer + payload_bytes_received;
					recv_len = remaining;
				}

				ssize_t bytes = recv(socket_fd, recv_dst, recv_len, 0);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::ConnectionLost;

				payload_bytes_received += static_cast<size_t>(bytes);

				if (receive_mode == ReceiveMode::StreamPayload && payload_reader && bytes > 0)
				{
					payload_reader(streamed_payload, static_cast<size_t>(bytes));
				}
			}

			if (receive_mode == ReceiveMode::StagePayload && payload_reader && header.payload_len > 0)
			{
				payload_reader(payload_buffer, header.payload_len);
			}

			log_preview("Received full message", header, nullptr, header.payload_len);
			stage = Stage::Completed;
			return Result::Completed;
		}

		return Result::Completed;
	}

} // namespace robotick
