#pragma once

#include <cstring>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

// Bump on ANY wire change, including field encodings. Both sides check for
// strict equality during the handshake.
constexpr uint32 PROTOCOL_VERSION = 2;

// Names are 32 bytes fixed on the wire: 31 characters plus the terminator.
constexpr uint32 MaxClientNameLength = 31;

// One byte on the wire. The value's range tells you the category:
//   1-63 requests (client->server), 64-127 events (server->client), 128+ data (both).
// Values 2 and 3 stay unused so pre-versioning clients never alias a current message.
enum MessageType
{
	// Requests
	SET_CLIENT_CONFIG = 1,
	GET_SERVER_INFO = 4,

	// Events
	SERVER_INFO = 64,
	CLIENT_LIST = 65,
	CLIENT_JOINED = 66,
	CLIENT_LEFT = 67,
	CLIENT_CONFIG_CHANGED = 68,

	// Data
	AUDIO = 128,
};

// Each side sets up lanes for what it sends. Lane 0 carries audio (most of
// the traffic; GNS advises lane 0 for that). Lane 1 carries requests and
// events, so audio and control never wait behind each other.
inline bool ConfigureLanes(ISteamNetworkingSockets* sockets, HSteamNetConnection conn)
{
	return sockets->ConfigureConnectionLanes(conn, 2, nullptr, nullptr) == k_EResultOK;
}

// Audio goes unreliable on lane 0, everything else reliable on lane 1.
// The type byte decides, so callers pass encoded bytes.
inline void SendToConnection(ISteamNetworkingSockets* sockets, HSteamNetConnection conn,
                             const void* bytes, uint32 size)
{
	bool audio = ((const uint8_t*)bytes)[0] == AUDIO;
	SteamNetworkingMessage_t* msg = SteamNetworkingUtils()->AllocateMessage(size);
	memcpy(msg->m_pData, bytes, size);
	msg->m_conn = conn;
	msg->m_nFlags = audio ? k_nSteamNetworkingSend_UnreliableNoNagle : k_nSteamNetworkingSend_ReliableNoNagle;
	msg->m_idxLane = audio ? 0 : 1;
	sockets->SendMessages(1, &msg, nullptr, true);
}
