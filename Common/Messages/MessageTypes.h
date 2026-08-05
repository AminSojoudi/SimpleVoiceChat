#pragma once

#include <steam/steamnetworkingsockets.h>

// Bump on ANY wire change, including field encodings. Both sides check for
// strict equality during the handshake.
constexpr uint32 PROTOCOL_VERSION = 1;

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
