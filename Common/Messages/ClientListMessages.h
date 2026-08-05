#pragma once

#include "MessageTypes.h"
#include "../Serialization/Serialize.h"

// Presence events. The server broadcasts these to every configured client;
// entries carry the channel and clients filter locally.

// Wire: channel i64, clientId u32, muted u8, name 32B = 45 bytes.
struct ClientEntry
{
	static constexpr uint32 WireSize = 45;

	int64 channel = 0;
	uint32 clientId = 0;
	uint8_t muted = 0;
	char name[MaxClientNameLength + 1] = {};

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_int64(stream, channel)) return false;
		if (!serialize_uint32(stream, clientId)) return false;
		if (!serialize_uint8(stream, muted)) return false;
		if (!serialize_name(stream, name)) return false;
		return true;
	}
};

// Full snapshot, sent to a client when its first config is accepted.
// clientCount doubles as the online count.
// Wire: type u8, clientCount u32 (range <= MaxClients), entries = 5 + 45 per entry.
struct ClientList
{
	static constexpr uint32 MaxClients = 64;
	static constexpr uint32 MaxWireSize = 5 + ClientEntry::WireSize * MaxClients;

	uint8_t type = CLIENT_LIST;
	uint32 clientCount = 0;
	ClientEntry entries[MaxClients];

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32_range(stream, clientCount, 0, MaxClients)) return false;
		for (uint32 i = 0; i < clientCount; ++i) {
			if (!entries[i].Serialize(stream)) return false;
		}
		return true;
	}
};

// Wire: type u8, ClientEntry = 46 bytes.
struct ClientJoined
{
	static constexpr uint32 MaxWireSize = 1 + ClientEntry::WireSize;

	uint8_t type = CLIENT_JOINED;
	ClientEntry entry;

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		return entry.Serialize(stream);
	}
};

// Wire: type u8, ClientEntry = 46 bytes.
struct ClientConfigChanged
{
	static constexpr uint32 MaxWireSize = 1 + ClientEntry::WireSize;

	uint8_t type = CLIENT_CONFIG_CHANGED;
	ClientEntry entry;

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		return entry.Serialize(stream);
	}
};

// Wire: type u8, clientId u32 = 5 bytes.
struct ClientLeft
{
	static constexpr uint32 MaxWireSize = 5;

	uint8_t type = CLIENT_LEFT;
	uint32 clientId = 0;

	template<typename Stream>
	bool Serialize(Stream& stream)
	{
		if (!serialize_uint8(stream, type)) return false;
		if (!serialize_uint32(stream, clientId)) return false;
		return true;
	}
};
