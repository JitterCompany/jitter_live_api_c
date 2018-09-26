#ifndef RECEIVE_H
#define RECEIVE_H

#include "live_api.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// poll regularly to handle tasks required for receiving
void receive(LiveAPI *ctx);

// handle incoming data (e.g. messages we are subscribed on)
bool receive_handle_incoming(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload);

#endif

