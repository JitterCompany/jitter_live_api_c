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

/**
 * Publish an ack message over MQTT.
 *
 * A thin wrapper around MQTT_client_publish,
 * to prefix the topic with "f/<username>/" and suffix it with "/ack"
 */
bool publish_mqtt_ack(LiveAPI *ctx, const char *relative_topic,
        const uint8_t *buffer, size_t sizeof_buffer);

// same as publish_mqtt_ack, but sends a progress message.
bool publish_mqtt_prog(LiveAPI *ctx, const char *relative_topic,
        const uint8_t *buffer, size_t sizeof_buffer);

#endif

