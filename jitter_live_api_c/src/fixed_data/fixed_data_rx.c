#include "live_api.h"
#include "fixed_data_rx.h"
#include "fixed_data_packet.h"
#include "mqtt.h"

// TODO move crc32 to some package? maybe c_utils?
#include "crc32.h"

#include "live_api_receive.h"
#include <c_utils/assert.h>

#include <string.h>

// publish a progress message every n messages
#define PROGRESS_INTERVAL (5)

// LiveAPIReceiveFixedState::send_events flags
enum ToSend {
    SEND_ACK        = (1 << 0),
    SEND_PROGRESS   = (1 << 1),
};

static void ack(LiveAPIReceiveFixedState *state, size_t offset)
{
    // ACK should be sent, nothing else (any pending SEND_PROGRESS is cleared)
    state->offset = offset;
    state->send_events = SEND_ACK;
}

static void update_progress(LiveAPIReceiveFixedState *state)
{
    if((state->offset % PROGRESS_INTERVAL) == 0) {
        state->send_events|= SEND_PROGRESS;
    }
}

/* Invalidate the transfer to an offset.
 *
 * Specify one of the following ofsets:
 *
 * - offset== state->offset: when a next packet makes the transfer valid again,
 *      the byte_offset and crc are still correct
 * - offset == 0xFFFF: transfer will never be valid again unless re-initialized
 * - offset = 0: transfer will be re-initialized next time
 *
 * All other values are useless, as the crc and bytes_total will not match.
 */
static void invalidate(LiveAPIReceiveFixedState *state, size_t offset)
{
    state->valid = false;
    state->offset = offset;
    if(!offset) {
        state->total = 0;
        state->byte_offset = 0;
        state->crc = 0;
    }
}

static void fail(LiveAPI *ctx, LiveAPIReceiveFixedState *state)
{
    // NOTE: because we only keep track of one state (e.g. not one per topic),
    // any 5 consecutive fails trigger a FORCE_ACK, even if they did not all
    // occur on the same topic.
    state->fail_count++;
    if(state->fail_count >= 5) {
        invalidate(state, 0);
        ctx->log_warning("live_api: force_ack for '%s': too many fails",
                state->topic->name);
        ack(state, 0xFFFF);
    }
}

static void reset_fails(LiveAPIReceiveFixedState *state)
{
    state->fail_count = 0;
}

void fixed_data_rx_handle_message(LiveAPI *ctx, const LiveAPITopic *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    FixedDataPacketHeader header;
    if(sizeof_payload < sizeof(header)) {
        ctx->log_warning("live_api: drop malformed packet for '%s'",
                topic->name);
        return;
    }
    LiveAPIReceiveFixedState *state = &ctx->fixed_receive_state;

    // NOTE: this is a limitation of this implementation. It is possible to
    // support receiving multiple fixeddata transfers back-to-back, but this
    // implementation needs a little bit of time to ack the last transfer
    // before we can process the next packet correctly. If this turns out to
    // be a problem in practice, send_events may be converted into some sort
    // of queue..?
    if(state->send_events & SEND_ACK) {
        ctx->log_warning("live_api: drop packet for '%s': busy sending ack",
                topic->name);
        return;
    }

    // parse payload into header + data
    memcpy(&header, payload, sizeof(header));
    uint8_t *data = payload + sizeof(header);
    size_t sizeof_data = sizeof_payload - sizeof(header);

    if(!header.packet_number) {
        ctx->log_debug("live_api: (re)starting fixed rx '%s'", topic->name);
        if(state->valid) {
            ctx->log_warning("live_api: removing old data from rx '%s'",
                    state->topic->name);
        }
        memset(state, 0, sizeof(*state));
    }

    if(!state->valid) {
        // packets start from zero: (re-)initialize
        if(!header.packet_number) {
            state->offset = 0;
            state->byte_offset = 0;
            state->total = header.total_packets;
            state->crc = 0;

            state->topic = topic;
            state->valid = true;
            state->send_events = 0;

        // same topic and expected packet number: mark as 'valid'
        } else if((state->topic == topic)
                && (header.packet_number == state->offset)) {
            state->valid = true;
        } else {

            ctx->log_debug("live_api: drop packet %u for '%s': unexpected id",
                    header.packet_number, topic->name);
            return;
        }
    }

    // Only one incoming fixeddata topic can be handled at a time.
    // If packets from multiple topics arrive interleaved, we cannot keep track
    // of both transfers: we drop one of them.
    //
    // NOTE: this is a limitation of this implementation. It is possible to
    // support receiving multiple fixeddata transfers simultaneously, but it
    // would require a bit more state management code.
    if(topic != state->topic) {
        ctx->log_warning("live_api: drop packet %u for '%s': busy on other topic",
                header.packet_number, topic->name);
        fail(ctx, state);
        ack(state, 0);
        invalidate(state, 0);
        return;
    }

    // should never happen: this is a malformed packet
    if(header.packet_number >= header.total_packets) {
        ctx->log_warning("live_api: drop packet %u for '%s': id out of bounds",
                header.packet_number, topic->name);
        fail(ctx, state);
        return;
    }

    // Edge case 3: total_packets changed unexpectedly
    // If packets from multiple topics arrive interleaved, we cannot keep track
    // of both transfers: we drop one of them.
    if(topic != state->topic) {
        ctx->log_warning("live_api: drop packet %u for '%s': busy on other topic",
                header.packet_number, topic->name);
        fail(ctx, state);
        ack(state, 0);
        invalidate(state, 0);
        return;
    }

    // Edge case 1: skipped packet(s)
    if(header.packet_number > state->offset) {
        ctx->log_warning("live_api: drop packet %u for '%s': expected %u",
                header.packet_number, topic->name, state->offset);
        fail(ctx, state);
        ack(state, state->offset);
        invalidate(state, state->offset);
        return;

    }
    
    // Edge case 4: duplicate packet(s)
    if(header.packet_number < state->offset) {
        ctx->log_warning("live_api: drop packet %u for '%s': already at %u",
                header.packet_number, topic->name, state->offset);
        return;
    }

    // packet_id is as expected: process this packet
    state->offset+= 1;
    update_progress(state);

    bool last_packet = false;
    uint32_t expected_crc = 0;

    // last packet!
    if(state->offset == state->total) {
        assert(header.packet_number == (state->offset -1));
        
       // Edge case: empty packet / missing CRC 
        if(sizeof_data < sizeof(expected_crc)) {
            ctx->log_warning("live_api: drop packets for '%s': CRC missing",
                    topic->name);
            fail(ctx, state);
            ack(state, 0);
            return;
        }

        last_packet = true;
        // last 4 bytes are the crc32 itself: adjust sizeof_data
        sizeof_data-= sizeof(expected_crc);
        memcpy(&expected_crc, &data[sizeof_data], sizeof(expected_crc));
    }
    // Update the crc
    state->crc = crc32b(data, sizeof_data, state->crc);

    if(last_packet) {

        // TODO Edge case 5: CRC mismatch
        if(expected_crc != state->crc) {
            ctx->log_warning("live_api: drop packets for '%s': CRC mismatch",
                    topic->name);
            fail(ctx, state);
            ack(state, 0);
            return;
        }
        ctx->log_debug("live_api: transfer complete for '%s'", topic->name);

        ack(state, state->offset);
        reset_fails(state);
        invalidate(state, 0);
    }

    // send data to user 'callback'
    live_api_receive_fixed(topic, data, sizeof_data,
            state->offset, last_packet);
    state->byte_offset+= sizeof_data;
}

void fixed_data_rx_update(LiveAPI *ctx)
{
    LiveAPIReceiveFixedState *state = &ctx->fixed_receive_state;

    if(state->send_events & SEND_ACK) {
        uint16_t ack = state->offset;
        if(publish_mqtt_ack(ctx, state->topic->name, (uint8_t*)&ack, sizeof(ack))) {
            state->send_events &= ~SEND_ACK;
        }
    } else if(state->send_events & SEND_PROGRESS) {
        uint16_t prog = state->offset;
        if(publish_mqtt_prog(ctx, state->topic->name, (uint8_t*)&prog, sizeof(prog))) {
            state->send_events &= ~SEND_PROGRESS;
        }
    }
}

