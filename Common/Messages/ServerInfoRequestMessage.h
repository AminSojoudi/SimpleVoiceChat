#pragma once


#include "MessageTypes.h"


// Client sends this to ask the server for its settings. Server answers with ServerInfo.
struct ServerInfoRequest
{
	uint8_t type = GET_SERVER_INFO;
};
