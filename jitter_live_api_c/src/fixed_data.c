#include "fixed_data.h"
#include "live_api.h"

// TODO move crc32 to some package? maybe c_utils?
#include "crc32.h"
#include "mqtt.h"

#include "live_api_receive.h"
#include <c_utils/round.h>
#include <c_utils/assert.h>

#include <string.h>
#include <stdio.h>



size_t fixed_data_calculate_num_packets(size_t num_data_bytes)
{
    const size_t num_bytes = sizeof(((LiveAPISendFixedState*)0)->crc)
        + num_data_bytes;
    return divide_round_up(num_bytes, LIVE_API_FIXED_DATA_PACKET_SIZE);
}

bool fixed_data_handle_message(LiveAPI *ctx, const LiveAPITopic *t,
        uint8_t *payload, const size_t sizeof_payload)
{
        bool is_complete = false;
        size_t byte_offset = 0;

        // TODO
        // - extract data
        // - calculate offset from header
        // - send ACK when appropriate
        // - determine when finished

        live_api_receive_fixed(t, payload, sizeof_payload,
                byte_offset, is_complete);
        return true;
}

bool fixed_data_handle_ack(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    LiveAPISendTask *task = &ctx->current_send_task;

    if(task->type != LIVE_API_TASK_FIXED_DATA) {
        return false;
    }

    const char *suffix = strrchr(topic, '/');
    if((suffix == NULL) || (0 != strcmp(suffix, "/ack"))) {
        return false;
    }

    char expected_topic[64];
    snprintf(expected_topic, sizeof(expected_topic), "%s/ack", task->topic);
    
    if(0 != strcmp(expected_topic, topic)) {
        ctx->log_warning("live_api: unexpected ack '%s'", topic);
        ctx->log_warning("live_api: expected '%s'", expected_topic);
        return false;
    }

    size_t ack = (payload[1] << 8) | (payload[0]);
    ctx->log_debug("live_api: rx ack=%u", ack);
    
    // Total should always be >= 1. If not, it is a bug..
    assert(ctx->fixed_send_state.total);

    if(ack >= ctx->fixed_send_state.total) {
        ctx->log_warning("live_api: got forced ack 0x%X on '%s'",
                ack, task->topic);
        ack = ctx->fixed_send_state.total - 1;
    }
    if((ack + 1) == ctx->fixed_send_state.total) {

        // All data is acked: done!
        live_api_send_queue_task_done(ctx->send_list, task->topic_id);
        ctx->log_debug("live_api: task '%s' is done", task->topic);
        task->type = LIVE_API_TASK_NONE;
    } else {
        ctx->log_debug("live_api: task '%s' busy %u/%u",
                task->topic, (ack+1), ctx->fixed_send_state.total);
    }

    ctx->fixed_send_state.offset = ack;
    return true;
}


void fixed_data_send(LiveAPI *ctx, LiveAPISendTask *task, const bool is_subtask)
{
    FixedDataPacket packet;

    // send the next chunk for this fixeddata task
    if(!is_subtask) {
        LiveAPISendFixedState *task_state = &ctx->fixed_send_state;
        if(task_state->offset >= task_state->total) {
            // all packets already sent, waiting for ack
            return;
        }
        packet.header.packet_number = task_state->offset;
        packet.header.total_packets = task_state->total;

        const bool is_last = ((packet.header.packet_number + 1)
                == packet.header.total_packets);

        // the last packet includes the crc32 checksum
        size_t max_bytes = sizeof(packet.data);
        if(is_last) {
            max_bytes-= sizeof(task_state->crc);
        }

        const size_t byte_offset = (task_state->offset
                * LIVE_API_FIXED_DATA_PACKET_SIZE);
        size_t len = live_api_send_queue_get_data(ctx->send_list,
                task->topic_id, packet.data, max_bytes, byte_offset);

        if(len > max_bytes) {
            len = max_bytes;
            ctx->log_error("live_api: too many bytes from get_data");
        }

        // calculate the new crc32 value
        uint32_t crc = crc32b(packet.data, len, task_state->crc);

        // Last packet: append crc32 to data
        if(is_last) {
            memcpy(packet.data+len, &crc, sizeof(crc));
        }

        if(publish_mqtt(ctx, task->topic, packet.as_bytes,
                    sizeof(packet.header) + len)) {
            ctx->fixed_send_state.offset+= 1;
            ctx->fixed_send_state.crc = crc;
            ctx->log_debug("live_api: published fixeddata packet %u/%u",
                    packet.header.packet_number,
                    packet.header.total_packets);
            return;
        }

    // this task is a subtask: only send an empty 'announce' packet
    } else {

        packet.header.packet_number = 0;

        // reserve space for all bytes including a crc32
        packet.header.total_packets = fixed_data_calculate_num_packets(task->size);

        // Send an empty packet at offset 0: just a header.
        // Eventually, we will re-send packet 0 with the actual contents
        if(publish_mqtt(ctx, task->topic, packet.as_bytes,
                    sizeof(packet.header))) {
            ctx->log_debug("live_api: published empty fixeddata packet %u/%u",
                    packet.header.packet_number,
                    packet.header.total_packets);
            return;
        }
    }
    ctx->log_warning("publish failed (type fixeddata)");
}

