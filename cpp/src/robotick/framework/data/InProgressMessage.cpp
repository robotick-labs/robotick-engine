// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/InProgressMessage.h"
#include "robotick/api.h"
#include "robotick/platform/Threading.h"

#include <errno.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#define ROBOTICK_REMOTE_ENGINE_LOG_PACKETS 0

namespace robotick
{

#if ROBOTICK_REMOTE_ENGINE_LOG_PACKETS
	static void log_preview(const char* label, const MessageHeader& header, const uint8_t* data, size_t size)
	{
		std::string preview;
		size_t max_preview = 64;
		size_t count = std::min(size, max_preview);
		for (size_t i = 0; i < count; ++i)
		{
			char c = static_cast<char>(data[i]);
			preview += (c != 0) ? c : '.';
		}
		if (size > max_preview)
			preview += "...";

		ROBOTICK_INFO("%s [%d] (%zu bytes, payload %zu bytes): '%s'", label, header.type, size, (size_t)header.payload_len, preview.c_str());
	}
#else
	static void log_preview(const char*, const MessageHeader&, const uint8_t*, size_t)
	{
	}
#endif

	void InProgressMessage::begin_send(uint8_t message_type, const uint8_t* data_ptr, const size_t data_size)
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_send() should only ever be called when empty");

		stage = Stage::Sending;
		header = {};
		std::memcpy(header.magic, MAGIC, 4);
		header.version = VERSION;
		header.type = message_type;
		header.payload_len = static_cast<uint32_t>(data_size);

		if (data_ptr != nullptr && data_size > 0)
		{
			payload.assign(static_cast<const uint8_t*>(data_ptr), static_cast<const uint8_t*>(data_ptr) + data_size);
		}
		else
		{
			payload.clear();
		}

		buffer.resize(sizeof(MessageHeader) + payload.size());
		std::memcpy(buffer.data(), &header, sizeof(MessageHeader));

		if (payload.size() > 0)
		{
			std::memcpy(buffer.data() + sizeof(MessageHeader), payload.data(), payload.size());
		}

		cursor = 0;
	}

	void InProgressMessage::begin_receive()
	{
		ROBOTICK_ASSERT(is_vacant() && "InProgressMessage::begin_send() should only ever be called when empty");

		stage = Stage::Receiving;
		buffer.resize(sizeof(MessageHeader));
		cursor = 0;
	}

	void InProgressMessage::vacate()
	{
		stage = Stage::Vacant;
		cursor = 0;
		buffer.clear();
		payload.clear();
	}

	InProgressMessage::Result InProgressMessage::tick(int socket_fd)
	{
		if (stage == Stage::Vacant)
			return Result::Completed;

		size_t total = buffer.size();
		size_t remaining = total - cursor;
		uint8_t* ptr = buffer.data() + cursor;

		ssize_t bytes = (stage == Stage::Sending) ? send(socket_fd, ptr, remaining, MSG_NOSIGNAL) : recv(socket_fd, ptr, remaining, 0);

		if (bytes < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// non-fatal; just try again later
				Thread::sleep_ms(1);
				return Result::InProgress;
			}
			return Result::ConnectionLost;
		}

		if (bytes == 0)
			return Result::ConnectionLost;

		cursor += bytes;

		if (cursor < total)
			return Result::InProgress;

		if (stage == Stage::Receiving && total == sizeof(MessageHeader))
		{
			// finished reading header — validate and prepare for payload
			std::memcpy(&header, buffer.data(), sizeof(MessageHeader));
			if (std::memcmp(header.magic, MAGIC, 4) != 0 || header.version != VERSION)
			{
				ROBOTICK_WARNING("InProgressMessage::tick(): Invalid header magic or version");
				return Result::ConnectionLost;
			}

			if (header.payload_len > 1024 * 1024)
			{
				ROBOTICK_WARNING("InProgressMessage::tick(): Payload too large (%u bytes)", (unsigned int)header.payload_len);
				return Result::ConnectionLost;
			}

			payload.resize(header.payload_len);
			buffer.resize(header.payload_len);
			cursor = 0;

			// handle zero-payload case immediately
			if (header.payload_len == 0)
			{
				log_preview("Received full message (no payload)", header, nullptr, 0);
				stage = Stage::Completed;
				return Result::Completed;
			}

			return Result::InProgress;
		}

		if (stage == Stage::Receiving)
		{
			// finished payload
			std::memcpy(payload.data(), buffer.data(), payload.size());
			log_preview("Received full message", header, payload.data(), payload.size());
		}
		else
		{
			log_preview("Sent full message", header, payload.data(), payload.size());
		}

		stage = Stage::Completed;

		return Result::Completed;
	}

} // namespace robotick
