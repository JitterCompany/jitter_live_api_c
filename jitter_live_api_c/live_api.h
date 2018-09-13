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
    LogFunc log_debug;
    LogFunc log_warning;
    LogFunc log_error;
    const char *client_id;
    char username[LIVE_API_MAX_USERNAME_LEN + 1];
    enum LiveAPIState state;
    LiveAPISendTask current_task;

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

void live_api_init(LiveAPI *ctx, StorageHAL storage,
        SocketHAL socket, TimestampFunc time_func,
        const char *client_id, LiveAPISendQueue *send_list);

void live_api_set_logging(LiveAPI *ctx,
        LogFunc debug, LogFunc warn, LogFunc err);
void live_api_poll(LiveAPI *ctx);

#endif

