// Copyright Robotick Labs
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

	void InProgressMessage::begin_send(uint8_t message_type, size_t payload_size_in, const PayloadWriter& writer)
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_send() should only ever be called when vacant");

		stage = Stage::Sending;
		header = {};

		static_assert(sizeof(MAGIC) == sizeof(header.magic));

		::memcpy(header.magic, MAGIC, sizeof(MAGIC));
		header.version = VERSION;
		header.type = message_type;
		header.payload_len = static_cast<uint32_t>(payload_size_in);

		header_bytes_sent = 0;
		payload_size = payload_size_in;
		payload_bytes_sent = 0;
		chunk_bytes_sent = 0;
		chunk_bytes_total = 0;
		payload_writer = writer;
	}

	void InProgressMessage::begin_receive(const PayloadReader& reader)
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_receive() should only ever be called when vacant");

		stage = Stage::Receiving;
		header = {};
		header_bytes_received = 0;
		payload_bytes_received = 0;
		payload_reader = reader;
	}

	void InProgressMessage::vacate()
	{
		stage = Stage::Vacant;
		header = {};
		header_bytes_sent = 0;
		payload_size = 0;
		payload_bytes_sent = 0;
		chunk_bytes_sent = 0;
		chunk_bytes_total = 0;
		header_bytes_received = 0;
		payload_bytes_received = 0;
		payload_writer = nullptr;
		payload_reader = nullptr;
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
			// 1) send header bytes first
			if (header_bytes_sent < sizeof(MessageHeader))
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
				if (header_bytes_sent < sizeof(MessageHeader))
					return Result::InProgress;
			}

			// 2) then stream payload
			if (payload_bytes_sent < payload_size)
			{
				if (chunk_bytes_sent == chunk_bytes_total)
				{
					size_t remaining = payload_size - payload_bytes_sent;
					const size_t max_to_write = min_size(sizeof(chunk_buffer), remaining);
					chunk_bytes_total = payload_writer ? payload_writer(payload_bytes_sent, chunk_buffer, max_to_write) : 0;
					chunk_bytes_sent = 0;

					if (chunk_bytes_total == 0)
					{
						ROBOTICK_WARNING("Payload writer returned zero bytes while data remains");
						return Result::ConnectionLost;
					}
				}

				const size_t remaining_chunk = chunk_bytes_total - chunk_bytes_sent;
				ssize_t bytes = send(socket_fd, chunk_buffer + chunk_bytes_sent, remaining_chunk, MSG_NOSIGNAL);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::InProgress;

				chunk_bytes_sent += static_cast<size_t>(bytes);
				payload_bytes_sent += static_cast<size_t>(bytes);

				if (chunk_bytes_sent < chunk_bytes_total)
					return Result::InProgress;
			}

			if (payload_bytes_sent < payload_size)
				return Result::InProgress;

			log_preview("Sent full message", header, nullptr, payload_size);
			stage = Stage::Completed;
			return Result::Completed;
		}

		// ---------------------------
		// Receiving path
		// ---------------------------
		if (stage == Stage::Receiving)
		{
			// 1) receive header
			if (header_bytes_received < sizeof(MessageHeader))
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
					return Result::InProgress;

				header.deserialize(header_bytes);

				static_assert(sizeof(MAGIC) == sizeof(header.magic));

				if (memcmp(header.magic, MAGIC, sizeof(MAGIC)) != 0 || header.version != VERSION)
				{
					ROBOTICK_WARNING("InProgressMessage::tick(): Invalid header magic or version");
					return Result::ConnectionLost;
				}

				if (header.payload_len > 1024 * 1024)
				{
					ROBOTICK_WARNING("InProgressMessage::tick(): Payload too large (%u bytes)", (unsigned int)header.payload_len);
					return Result::ConnectionLost;
				}
			}

			// 2) receive payload
			if (payload_bytes_received < header.payload_len)
			{
				const size_t remaining = header.payload_len - payload_bytes_received;
				const size_t to_read = min_size(sizeof(chunk_buffer), remaining);

				ssize_t bytes = recv(socket_fd, chunk_buffer, to_read, 0);

				if (bytes < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						return Result::InProgress;
					return Result::ConnectionLost;
				}

				if (bytes == 0)
					return Result::ConnectionLost;

				payload_bytes_received += static_cast<size_t>(bytes);
				if (payload_reader && bytes > 0)
				{
					payload_reader(chunk_buffer, static_cast<size_t>(bytes));
				}

				if (payload_bytes_received < header.payload_len)
					return Result::InProgress;
			}

			log_preview("Received full message", header, nullptr, header.payload_len);
			stage = Stage::Completed;
			return Result::Completed;
		}

		return Result::Completed;
	}

} // namespace robotick
