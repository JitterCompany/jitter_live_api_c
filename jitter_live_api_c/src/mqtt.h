#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "live_api.h"

/**
 * Publish a message over MQTT.
 *
 * A thin wrapper around MQTT_client_publish,
 * to prefix the topic with "f/<username>/"
 */
bool publish_mqtt(LiveAPI *ctx, const char *relative_topic,
        const uint8_t *buffer, size_t sizeof_buffer);

#endif

