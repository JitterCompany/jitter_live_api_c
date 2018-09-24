#ifndef FIXED_DATA_H
#define FIXED_DATA_H

#include "live_api.h"
#include <stdbool.h>
#include <stddef.h>

// Try to make send progress on the current fixed_data task
void fixed_data_send(LiveAPI *ctx, LiveAPISendTask *task, const bool is_subtask);

// Calculate how many packets are required to send a given number of bytes.
// The calculation includes the extra bytes required for the CRC checksum.
size_t fixed_data_calculate_num_packets(size_t num_data_bytes);

// Detect if the given topic is an 'ack' topic, try to handle the ack if valid
bool fixed_data_handle_ack(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload);

#endif

