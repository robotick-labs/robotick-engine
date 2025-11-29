// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/Function.h"
#include "robotick/framework/data/MessageHeader.h"

#include <cstdint>
#include <cstring>

namespace robotick
{
	constexpr char MAGIC[4] = {'R', 'B', 'I', 'N'};
	constexpr uint8_t VERSION = 1;

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

		enum class Result
		{
			InProgress,
			Completed,
			ConnectionLost
		};

		using PayloadWriter = Function<size_t(size_t offset, uint8_t* dst, size_t max_len)>;
		using PayloadReader = Function<void(const uint8_t* data, size_t len)>;

		void begin_send(uint8_t message_type, size_t payload_size, const PayloadWriter& writer);
		void begin_receive(const PayloadReader& reader);

		bool is_vacant() const { return stage == Stage::Vacant; }
		bool is_occupied() const { return stage != Stage::Vacant; }
		bool is_completed() const { return stage == Stage::Completed; }
		uint32_t payload_length() const { return header.payload_len; }

		InProgressMessage::Result tick(int socket_fd);
		void vacate();

	  private:
		Stage stage = Stage::Vacant;
		MessageHeader header{};

		// send state
		size_t header_bytes_sent = 0;
		size_t payload_size = 0;
		size_t payload_bytes_sent = 0;
		size_t chunk_bytes_sent = 0;
		size_t chunk_bytes_total = 0;
		PayloadWriter payload_writer;

		// receive state
		size_t header_bytes_received = 0;
		size_t payload_bytes_received = 0;
		PayloadReader payload_reader;

		// temporary buffer reused for both send & receive payload chunks
		uint8_t chunk_buffer[1024]{};
	};

} // namespace robotick
