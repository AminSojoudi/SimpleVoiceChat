#ifndef SET_TOPIC_MESSAGE_H
#define SET_TOPIC_MESSAGE_H


#include <string>


struct SetTopicData
{
	MessageType type = SET_TOPIC;
	std::string topic;
};



#endif //SET_TOPIC_MESSAGE_H