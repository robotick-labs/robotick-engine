// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/MessageHeader.h"
#include "robotick/framework/utility/Function.h"

#include <cstdint>
#include <cstring>

namespace robotick
{
	// Incremental send/receive helper that lets RemoteEngineConnection share a single scratch buffer across non-blocking sockets.
	// Each instance owns one half-duplex stream (either outbound or inbound) so we can make progress on both directions without
	// forcing the caller to juggle partial headers/payloads manually.
	class InProgressMessage
	{
	  public:
		InProgressMessage() = default;
		~InProgressMessage() { delete[] payload_buffer; }

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

		enum class ReceiveMode
		{
			StagePayload,
			StreamPayload
		};

		using PayloadWriter = Function<size_t(size_t offset, uint8_t* dst, size_t max_len)>;
		using PayloadReader = Function<void(const uint8_t* data, size_t len)>;

		void begin_send(uint8_t message_type, size_t payload_size, const PayloadWriter& writer);
		void begin_receive(const PayloadReader& reader, ReceiveMode receive_mode = ReceiveMode::StagePayload);

		bool is_vacant() const { return stage == Stage::Vacant; }
		bool is_occupied() const { return stage != Stage::Vacant; }
		bool is_completed() const { return stage == Stage::Completed; }
		uint32_t payload_length() const { return header.payload_len; }
		void reserve_payload_capacity(size_t payload_capacity);

		Result tick(int socket_fd);
		void vacate();

	  private:
		static constexpr char kMagic[4] = {'R', 'B', 'I', 'N'};
		static constexpr uint8_t kVersion = 1;
		static constexpr size_t kMaxPayloadBytes = 1024 * 1024;

		Stage stage = Stage::Vacant;
		MessageHeader header{};
		uint8_t* payload_buffer = nullptr;
		size_t payload_buffer_capacity = 0;

		// send state
		size_t header_bytes_sent = 0;
		size_t payload_size = 0;
		size_t payload_bytes_sent = 0;

		// receive state
		size_t header_bytes_received = 0;
		size_t payload_bytes_received = 0;
		PayloadReader payload_reader;
		ReceiveMode receive_mode = ReceiveMode::StagePayload;
	};

} // namespace robotick
