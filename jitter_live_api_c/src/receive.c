#include "receive.h"
#include "fixed_data/fixed_data_rx.h"

// 'callbacks' to library user
#include "live_api_receive.h"

#include <string.h>

// forward declarations
//
static const LiveAPITopic * find_topic(const LiveAPI *ctx, const char *topic);

void receive(LiveAPI *ctx)
{
    fixed_data_rx_update(ctx);
}

bool receive_handle_incoming(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    const LiveAPITopic *t= find_topic(ctx, topic);
    if(!t) {
        return false;
    }

    ctx->log_debug("live_api: receive msg on '%s'", topic);
    if(t->type == LIVE_API_TASK_PLAIN) {
        live_api_receive(t, payload, sizeof_payload);
        return true;
    }
    if(t->type == LIVE_API_TASK_FIXED_DATA) {
        fixed_data_rx_handle_message(ctx, t, payload, sizeof_payload);
        return true;
    }
    return false;
}

static const LiveAPITopic * find_topic(const LiveAPI *ctx, const char *topic)
{
    if(!ctx->rx_topics) {
        return NULL;
    }

    const LiveAPITopic *t= ctx->rx_topics;
    for(size_t i=0;i<ctx->rx_topic_count;i++,t++) {
        if(t->name && (0 == strcmp(t->name, topic))) {
            return t;
        }
    }
    return NULL;
}
