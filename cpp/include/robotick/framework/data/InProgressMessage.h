// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/MessageHeader.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>

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

		void begin_send(uint8_t message_type, const uint8_t* data_ptr, const size_t data_size);
		void begin_receive();

		bool is_vacant() const { return stage == Stage::Vacant; }
		bool is_occupied() const { return stage != Stage::Vacant; }
		bool is_completed() const { return stage == Stage::Completed; }

		std::tuple<const uint8_t*, size_t> get_payload() const;

		InProgressMessage::Result tick(int socket_fd);
		void vacate();

	  private:
		Stage stage = Stage::Vacant;
		size_t cursor = 0;
		MessageHeader header{};
		std::vector<uint8_t> buffer;
	};

	inline std::tuple<const uint8_t*, size_t> InProgressMessage::get_payload() const
	{
		if (buffer.size() <= sizeof(MessageHeader))
			return {nullptr, 0};

		return {buffer.data() + sizeof(MessageHeader), buffer.size() - sizeof(MessageHeader)};
	}

} // namespace robotick
