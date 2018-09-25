#ifndef LIVE_API_H
#define LIVE_API_H

#include <mqtt_c/mqtt_client.h>

#include "live_api_send_queue.h"
#include "storage_HAL.h"

#ifndef LIVE_API_MAX_USERNAME_LEN
    #define LIVE_API_MAX_USERNAME_LEN 15
#endif

#ifndef LIVE_API_MAX_SEND_LEN
    #define LIVE_API_MAX_SEND_LEN 200
#endif

enum LiveAPIState {

    LIVE_API_NONE = 0,
    LIVE_API_SUBSCRIBE,

    // skip if registered
    LIVE_API_SUBSCRIBE_REG,
    LIVE_API_REGISTER,
    LIVE_API_REGISTER_WAIT,

    // skip if verified
    LIVE_API_VERIFY,
    LIVE_API_VERIFY_WAIT,

    // TODO insert testmode states here?
    

    LIVE_API_HI,
    
    // at this point, the API is ready for use
    LIVE_API_IDLE,

    LIVE_API_WANT_OFFLINE,
    LIVE_API_WANT_OFFLINE_WAIT,

    LIVE_API_BYE,
    LIVE_API_BYE_WAIT,
    LIVE_API_ERROR,
};
typedef struct {
    size_t offset;
    size_t total;
    uint32_t crc;
} LiveAPISendFixedState;

typedef struct {
    size_t offset;
    size_t total;
    uint32_t crc;
    
    const LiveAPITopic *topic;
    bool valid;
    size_t fail_count;

} LiveAPIReceiveFixedState;


typedef struct {
    LogFunc log_debug;
    LogFunc log_warning;
    LogFunc log_error;
    const char *client_id;
    const char *server_addr;
    int server_port;
    char username[LIVE_API_MAX_USERNAME_LEN + 1];
    enum LiveAPIState state;

    // send state
    LiveAPISendTask current_send_task;
    LiveAPISendFixedState fixed_send_state;         // fixed-size send state

    // receive state
    LiveAPIReceiveFixedState fixed_receive_state;   // fixed-size receive state

    // a list of topics to pass to live_api_receive()
    const LiveAPITopic *rx_topics;
    size_t rx_topic_count;

    // periodically try to go offline
    TimestampFunc time_func;
    uint64_t offline_timestamp;

    // TODO: do we need this runtime-configurable?
    // alternative: depend on hardcoded function names to be implemented
    // by the user
    StorageHAL storage;
    LiveAPISendQueue *send_list;
    MQTTClient mqtt;
} LiveAPI;


/**
 * Initialize the live_api
 *
 * NOTE: parameters client_id, server_addr and send_list should point to memory
 * that stays valid during the lifetime of the LiveAPI object: they will be
 * accessed after live_api_init() returns.
 */
void live_api_init(LiveAPI *ctx, StorageHAL storage,
        SocketHAL socket, TimestampFunc time_func,
        const char *client_id, const char *server_addr, const int server_port,
        LiveAPISendQueue *send_list);

void live_api_set_logging(LiveAPI *ctx,
        LogFunc debug, LogFunc warn, LogFunc err);
void live_api_update_subscriptions(LiveAPI *ctx,
        const LiveAPITopic *topic, size_t num_topics);
void live_api_poll(LiveAPI *ctx);

#endif

