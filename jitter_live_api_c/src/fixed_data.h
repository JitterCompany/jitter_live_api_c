#ifndef FIXED_DATA_H
#define FIXED_DATA_H

#include "live_api.h"
#include <stdbool.h>
#include <stddef.h>

void fixed_data_send(LiveAPI *ctx, LiveAPISendTask *task, const bool is_subtask);
size_t fixed_data_calculate_num_packets(size_t num_data_bytes);
bool fixed_data_handle_ack(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload);

#endif

