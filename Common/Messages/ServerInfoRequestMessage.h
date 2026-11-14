#pragma once

#include "MessageTypes.h"
#include "../Serialization/Serialize.h"

// Client sends this to ask the server for its settings. Server answers with ServerInfo.
// This is the first message a client sends, so it carries the protocol version.
// Wire: type u8, protocolVersion u32 = 5 bytes.
struct ServerInfoRequest
{
	static constexpr uint32 MaxWireSize = 5;

	uint8_t type = GET_SERVER_INFO;
	uint32 protocolVersion = PROTOCOL_VERSION;

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32(stream, protocolVersion)) return false;
		return true;
	}
};
