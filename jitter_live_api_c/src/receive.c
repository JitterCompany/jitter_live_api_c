#include "receive.h"

#include <string.h>

// forward declarations
//
static const LiveAPITopic * find_topic(const LiveAPI *ctx, const char *topic);


bool receive_handle_incoming(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    const LiveAPITopic *t= find_topic(ctx, topic);
    if(!t) {
        return false;
    }

    ctx->log_debug("live_api: receive msg on '%s'", topic);
    // TODO handle the message over to the user, handle fixeddata etc
    //
    return true;
}

static const LiveAPITopic * find_topic(const LiveAPI *ctx, const char *topic)
{
    if(!ctx->rx_topics) {
        return NULL;
    }

    const LiveAPITopic *t= ctx->rx_topics;
    for(size_t i=0;i<ctx->rx_topic_count;i++,t++) {
        if(t->topic && (0 == strcmp(t->topic, topic))) {
            return t;
        }
    }
    return NULL;
}
