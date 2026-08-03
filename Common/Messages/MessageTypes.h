#pragma once

#include <steam/steamnetworkingsockets.h>


enum MessageType
{
	SET_CHANNEL = 1,
	AUDIO = 2,
	GET_SERVER_INFO = 3,
	SERVER_INFO = 4
};