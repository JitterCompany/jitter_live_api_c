#ifndef LIVE_API_RECEIVE_H
#define LIVE_API_RECEIVE_H

#include "live_api_topic.h"
#include <stdint.h>
#include <stddef.h>

void live_api_receive(const LiveAPITopic *topic,
        uint8_t *payload, size_t sizeof_payload);
void live_api_receive_fixed(const LiveAPITopic *topic,
        uint8_t *payload, size_t sizeof_payload,
        size_t byte_offset, bool complete);


#endif

