#pragma once


#include <string>


struct SetTopicData
{
	uint8_t type = SET_TOPIC;
	std::string topic;
};