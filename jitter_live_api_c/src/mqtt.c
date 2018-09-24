#include "mqtt.h"
#include <stdio.h>

bool publish_mqtt(LiveAPI *ctx, const char *relative_topic,
        const uint8_t *buffer, size_t sizeof_buffer)
{
    char topic[64];
    snprintf(topic, sizeof(topic),
            "f/%s/%s", ctx->username, relative_topic);

    return MQTT_client_publish(&ctx->mqtt, topic, buffer, sizeof_buffer);
}
