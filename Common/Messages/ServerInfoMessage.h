#pragma once


#include "MessageTypes.h"


// Server answer to GetServerInfo. Clients need these settings before they start audio.
struct ServerInfo
{
	uint8_t type = SERVER_INFO;
	uint32 sampleRate;
};
