// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <arpa/inet.h>
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
			uint16_t net_reserved = htons(reserved);
			uint32_t net_payload_len = htonl(payload_len);

			memcpy(out, magic, 4);
			out[4] = version;
			out[5] = type;
			out[6] = net_reserved >> 8;
			out[7] = net_reserved & 0xFF;
			out[8] = net_payload_len >> 24;
			out[9] = (net_payload_len >> 16) & 0xFF;
			out[10] = (net_payload_len >> 8) & 0xFF;
			out[11] = net_payload_len & 0xFF;
		}

		inline void deserialize(const uint8_t* in)
		{
			memcpy(magic, in, 4);
			version = in[4];
			type = in[5];
			uint16_t net_reserved = (in[6] << 8) | in[7];
			uint32_t net_payload_len = (in[8] << 24) | (in[9] << 16) | (in[10] << 8) | in[11];

			reserved = ntohs(net_reserved);
			payload_len = ntohl(net_payload_len);
		}
	};
#pragma pack(pop)

	static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be 12 bytes");

} // namespace robotick
