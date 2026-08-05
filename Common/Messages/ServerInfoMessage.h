#pragma once

#include "MessageTypes.h"
#include "../Serialization/Serialize.h"

// Server answer to GetServerInfo. Clients need these settings before they start audio.
// Wire: type u8, sampleRate u32, protocolVersion u32 = 9 bytes.
struct ServerInfo
{
	static constexpr uint32 MaxWireSize = 9;

	uint8_t type = SERVER_INFO;
	uint32 sampleRate = 0;
	uint32 protocolVersion = PROTOCOL_VERSION;

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32(stream, sampleRate)) return false;
		if (!serialize_uint32(stream, protocolVersion)) return false;
		return true;
	}
};
