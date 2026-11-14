#pragma once

#include "MessageTypes.h"
#include "../Serialization/Serialize.h"

// Server answer to GetServerInfo. Clients need these settings before they start audio.
// Wire: type u8, sampleRate u32, protocolVersion u32, clientId u32 = 13 bytes.
struct ServerInfo
{
	static constexpr uint32 MaxWireSize = 13;

	uint8_t type = SERVER_INFO;
	uint32 sampleRate = 0;
	uint32 protocolVersion = PROTOCOL_VERSION;
	uint32 clientId = 0; // the receiving client's own id

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32(stream, sampleRate)) return false;
		if (!serialize_uint32(stream, protocolVersion)) return false;
		if (!serialize_uint32(stream, clientId)) return false;
		return true;
	}
};
