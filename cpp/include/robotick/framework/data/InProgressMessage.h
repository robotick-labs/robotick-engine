// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace robotick
{
	constexpr const char* MAGIC = "RBIN";
	constexpr uint8_t VERSION = 1;

	struct MessageHeader
	{
		char magic[4]; // 'RBIN'
		uint8_t version;
		uint8_t type;
		uint16_t reserved;
		uint32_t payload_len;
	};

	static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be 12 bytes");

	class InProgressMessage
	{
	  public:
		enum class Stage
		{
			Vacant,
			Sending,
			Receiving,
			Completed
		};

		enum class Result { InProgress, Completed, ConnectionLost };

		void begin_send(uint8_t message_type, const uint8_t* data_ptr, const size_t data_size);
		void begin_receive();

		bool is_vacant() const { return stage == Stage::Vacant; }
		bool is_occupied() const { return stage != Stage::Vacant; }
		bool is_completed() const { return stage == Stage::Completed; }

		const MessageHeader& get_header() const { return header; }
		const std::vector<uint8_t>& get_payload() const { return payload; }

		InProgressMessage::Result tick(int socket_fd);
		void vacate();

	  private:
		Stage stage = Stage::Vacant;
		size_t cursor = 0;
		MessageHeader header{};
		std::vector<uint8_t> buffer;
		std::vector<uint8_t> payload;
	};

} // namespace robotick
