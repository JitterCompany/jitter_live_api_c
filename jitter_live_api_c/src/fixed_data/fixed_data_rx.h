#ifndef FIXED_DATA_RX_H
#define FIXED_DATA_RX_H

#include "live_api.h"
#include <stdbool.h>
#include <stddef.h>



// Try to make receive progress on the current fixed_data task
void fixed_data_rx_update(LiveAPI *ctx);

// Handle incoming message: chunk of a file
void fixed_data_rx_handle_message(LiveAPI *ctx, const LiveAPITopic *t,
        uint8_t *payload, const size_t sizeof_payload);

#endif

