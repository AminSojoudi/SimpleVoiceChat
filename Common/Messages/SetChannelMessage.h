#pragma once


#include <string>


struct SetChannel
{
	uint8_t type = SET_CHANNEL;
	int64 channel;
};