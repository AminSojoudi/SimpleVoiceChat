#pragma once

#include "MessageTypes.h"
#include "../Serialization/Serialize.h"

// Client -> server settings. Add future client-to-server config fields here.
// Wire: type u8, protocolVersion u32, channel i64, loopback u8, muted u8, name 32B = 47 bytes.
struct SetClientConfig
{
	static constexpr uint32 MaxWireSize = 47;

	uint8_t type = SET_CLIENT_CONFIG;
	uint32 protocolVersion = PROTOCOL_VERSION;
	int64 channel = 0;
	uint8_t loopback = 0;	// 1 = client wants to hear their own voice back
	uint8_t muted = 0;		// carried on the wire; nothing sets it until phase 3
	char name[MaxClientNameLength + 1] = {};

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32(stream, protocolVersion)) return false;
		if (!serialize_int64(stream, channel)) return false;
		if (!serialize_uint8(stream, loopback)) return false;
		if (!serialize_uint8(stream, muted)) return false;
		if (!serialize_name(stream, name)) return false;
		return true;
	}
};
