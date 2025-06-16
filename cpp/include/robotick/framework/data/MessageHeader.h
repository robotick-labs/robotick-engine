// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstring>

namespace robotick
{

#pragma pack(push, 1)
	struct MessageHeader
	{
		char magic[4]; // 'RBIN'
		uint8_t version;
		uint8_t type;
		uint16_t reserved;
		uint32_t payload_len;

		inline void serialize(uint8_t* out) const
		{
			memcpy(out, magic, 4);
			out[4] = version;
			out[5] = type;
			out[6] = reserved >> 8;
			out[7] = reserved & 0xFF;
			out[8] = payload_len >> 24;
			out[9] = (payload_len >> 16) & 0xFF;
			out[10] = (payload_len >> 8) & 0xFF;
			out[11] = payload_len & 0xFF;
		}

		inline void deserialize(const uint8_t* in)
		{
			memcpy(magic, in, 4);
			version = in[4];
			type = in[5];
			reserved = (in[6] << 8) | in[7];
			payload_len = (in[8] << 24) | (in[9] << 16) | (in[10] << 8) | in[11];
		}
	};
#pragma pack(pop)

	static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be 12 bytes");

} // namespace robotick
