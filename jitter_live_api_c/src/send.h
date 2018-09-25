#ifndef SEND_H
#define SEND_H

#include "live_api.h"

// try to get a current_send_task to work on
bool send_update_current_task(LiveAPI *ctx);

// handle incoming data (e.g. acks)
bool send_handle_incoming(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload);

// handle sending data: publish data based on currently active tasks in sendqueue
void send(LiveAPI *ctx);


#endif

