#include "live_api.h"
#include <string.h>
#include <stdio.h>
#include "live_api_parse.h"

// TODO tune these values
#define KEEPALIVE_INTERVAL 30
#define MQTT_TIMEOUT 10
#define OFFLINE_REQUEST_INTERVAL 5


typedef struct {
    char username[LIVE_API_MAX_USERNAME_LEN];
    char password[64];
} LoginCredentials;

// forward declarations

// internal state management
static const char *state_to_str(enum LiveAPIState state);
static void set_state(LiveAPI *ctx, enum LiveAPIState new_state);
static void handle_state_machine(LiveAPI *ctx);
static void ensure_connection(LiveAPI *ctx);
static bool update_current_task(LiveAPI *ctx);

// state handlers
static void state_void(LiveAPI *ctx){}
static void state_subscribe(LiveAPI *ctx);
static void state_subscribe_reg(LiveAPI *ctx);
static void state_register(LiveAPI *ctx);
static void state_verify(LiveAPI *ctx);
static void state_hi(LiveAPI *ctx);
static void state_idle(LiveAPI *ctx);
static void state_want_offline(LiveAPI *ctx);
static void state_bye(LiveAPI *ctx);
static void state_bye_wait(LiveAPI *ctx);
static void state_error(LiveAPI *ctx);

// other internal functions
static void dummy_log(const char format[], ...){}
static void on_message(void *void_ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload);
static bool get_login_credentials(const LiveAPI *ctx, LoginCredentials *result);
static bool is_verified(const LiveAPI *ctx);


typedef void(*LiveAPIStateFunc)(LiveAPI *ctx);
static const LiveAPIStateFunc g_state[] = {
    [LIVE_API_NONE]             = state_void,
    [LIVE_API_SUBSCRIBE]        = state_subscribe,

    // these states are skipped depending on register/verify status
    [LIVE_API_SUBSCRIBE_REG]    = state_subscribe_reg,
    [LIVE_API_REGISTER]         = state_register,
    [LIVE_API_REGISTER_WAIT]    = state_void,
    [LIVE_API_VERIFY]           = state_verify,
    [LIVE_API_VERIFY_WAIT]      = state_void,

    [LIVE_API_HI]               = state_hi,
    [LIVE_API_IDLE]             = state_idle,

    [LIVE_API_WANT_OFFLINE]     = state_want_offline,
    [LIVE_API_WANT_OFFLINE_WAIT]= state_void,
    [LIVE_API_BYE]              = state_bye,
    [LIVE_API_BYE_WAIT]         = state_bye_wait,

    [LIVE_API_ERROR]            = state_error,
};
#define NUM_STATES (sizeof(g_state)/sizeof(g_state[0]))


void live_api_init(LiveAPI *ctx, StorageHAL storage,
        SocketHAL socket, TimestampFunc time_func,
        const char *client_id, const char *server_addr, const int server_port,
        LiveAPISendQueue *send_list)
{
    ctx->log_debug = dummy_log;
    ctx->log_warning = dummy_log;
    ctx->log_error = dummy_log;

    ctx->client_id = client_id;
    ctx->server_addr = server_addr;
    ctx->server_port = server_port;
    memset(ctx->username, 0, sizeof(ctx->username));
    ctx->state = LIVE_API_NONE;
    memset(&ctx->current_task, 0, sizeof(LiveAPISendTask));

    ctx->time_func = time_func;
    ctx->offline_timestamp = 0;
    ctx->storage = storage;
    ctx->send_list = send_list;
    MQTT_client_init(&ctx->mqtt, socket, time_func,
            on_message, ctx,
            KEEPALIVE_INTERVAL, MQTT_TIMEOUT);
}

void live_api_set_logging(LiveAPI *ctx,
        LogFunc debug, LogFunc warn, LogFunc err)
{
    MQTT_client_set_logging(&ctx->mqtt, debug, warn, err);

    if(debug) {
        ctx->log_debug = debug;
    }
    if(warn) {
        ctx->log_warning = warn;
    }
    if(err) {
        ctx->log_error = err;
    }
}

void live_api_poll(LiveAPI *ctx)
{
    if(update_current_task(ctx)) {
        ensure_connection(ctx);
    }

    if(MQTT_client_poll(&ctx->mqtt)) {
        handle_state_machine(ctx);
    }
}

// try to get a current_task to work on
static bool update_current_task(LiveAPI *ctx)
{
    if(ctx->current_task.type != LIVE_API_TASK_NONE) {
        return true;
    }
    return live_api_send_queue_get_task(ctx->send_list, &ctx->current_task);
}

// ensure there is a connection: create a new connection if required
static void ensure_connection(LiveAPI *ctx)
{
    if(!MQTT_client_is_disconnected(&ctx->mqtt)) {
        return;
    }
    
    ctx->log_debug("client is disconected: let's try to connect...");

    LoginCredentials credentials;
    const bool has_login = get_login_credentials(ctx, &credentials);
    if(has_login) {
        ctx->log_debug("live_api: credentials are valid");

        // cache username in ctx->username as string
        memcpy(ctx->username,
                credentials.username, LIVE_API_MAX_USERNAME_LEN);
        ctx->username[LIVE_API_MAX_USERNAME_LEN] = 0;

        char will_topic[64];
        snprintf(will_topic, sizeof(will_topic), "f/%s/bye", ctx->username);
        const MQTTWill will = {
            .topic = will_topic,
            .payload = NULL,
            .sizeof_payload = 0,
            .retain = false
        };

        // connect with username+pw
        if(!MQTT_client_connect(&ctx->mqtt,
                    ctx->server_addr, ctx->server_port, ctx->client_id,
                    credentials.username, credentials.password,
                    &will)) {
            ctx->log_warning("live_api: MQTT connect failed!");
            return;
        }

        set_state(ctx, LIVE_API_SUBSCRIBE);

    } else {
        // connect anonymous: register for new login credentials
        if(!MQTT_client_connect(&ctx->mqtt, ctx->server_addr, ctx->server_port,
                    ctx->client_id, NULL, NULL, NULL)) {
            ctx->log_warning("live_api: registering: MQTT connect failed!");
            return;
        }

        set_state(ctx, LIVE_API_SUBSCRIBE_REG);
        return;
    }
}

static void handle_state_machine(LiveAPI *ctx)
{
    // handle state machine
    if(NULL == g_state[ctx->state]) {
        ctx->log_error("unhandled state '%s'!", state_to_str(ctx->state));
        set_state(ctx, LIVE_API_ERROR);
        return;
    }
    g_state[ctx->state](ctx);
}

static const char *state_to_str(enum LiveAPIState state)
{
    switch(state) {
        case LIVE_API_NONE:
            return "NONE";
        case LIVE_API_SUBSCRIBE:
            return "SUBSCRIBE";
        case LIVE_API_SUBSCRIBE_REG:
            return "SUBSCRIBE_REG";
        case LIVE_API_REGISTER:
            return "REGISTER";
        case LIVE_API_REGISTER_WAIT:
            return "REGISTER_WAIT";
        case LIVE_API_VERIFY:
            return "VERIFY";
        case LIVE_API_VERIFY_WAIT:
            return "VERIFY_WAIT";
        case LIVE_API_HI:
            return "HI";
        case LIVE_API_IDLE:
            return "IDLE";
        case LIVE_API_WANT_OFFLINE:
            return "WANT_OFFLINE";
        case LIVE_API_WANT_OFFLINE_WAIT:
            return "WANT_OFFLINE_WAIT";
        case LIVE_API_BYE:
            return "BYE";
        case LIVE_API_BYE_WAIT:
            return "BYE_WAIT";
        case LIVE_API_ERROR:
            return "ERROR";

        // no default: compiler should warn if a new state is added
        // without explicitly handling it here
    }
    return "UNKNOWN";
}

static void set_state(LiveAPI *ctx, enum LiveAPIState new_state)
{
    if(new_state == ctx->state) {
        return;
    }
    if(new_state > NUM_STATES) {

        ctx->log_error("live_api: BUG! unknown state %d!", (int)new_state);
        set_state(ctx, LIVE_API_ERROR);
        return;
    }

    if(new_state == LIVE_API_ERROR) {
        ctx->log_debug("live_api: error while in state %s",
                state_to_str(ctx->state));
    } else {
        ctx->log_debug("live_api: state to %s", state_to_str(new_state));
    }
    // (re-)init ofline interval when going to IDLE state
    if(new_state == LIVE_API_IDLE) {
        ctx->offline_timestamp = ctx->time_func();
    }
    ctx->state = new_state;
}

static void state_subscribe(LiveAPI *ctx)
{
    // subscribe on all topics published to our username
    char topic_filter[64];
    snprintf(topic_filter, sizeof(topic_filter),
            "t/%s/#", ctx->username);
    if(MQTT_client_subscribe(&ctx->mqtt, topic_filter)) {
        if(!is_verified(ctx)) {
            set_state(ctx, LIVE_API_VERIFY);
        } else {
            set_state(ctx, LIVE_API_HI);
        }
    }
}

static void state_subscribe_reg(LiveAPI *ctx)
{
    // subscribe while registering:
    // we are logged in as anonymous, so we subscribe on clientid topics
    char topic_filter[64];
    snprintf(topic_filter, sizeof(topic_filter),
            "t/client-%s/#", ctx->client_id);
    if(MQTT_client_subscribe(&ctx->mqtt, topic_filter)) {
        set_state(ctx, LIVE_API_REGISTER);
    }
}

static void state_register(LiveAPI *ctx)
{
    char topic[64];
    snprintf(topic, sizeof(topic),
            "f/client-%s/register", ctx->client_id);
    if(MQTT_client_publish(&ctx->mqtt, topic, NULL, 0)) {
        ctx->log_debug("live_api: sent 'register' request...");
        set_state(ctx, LIVE_API_REGISTER_WAIT);
    }
}

static void state_verify(LiveAPI *ctx)
{
    char topic[64];
    snprintf(topic, sizeof(topic),
            "f/%s/verify", ctx->username);
    if(MQTT_client_publish(&ctx->mqtt, topic, NULL, 0)) {
        ctx->log_debug("live_api: sent 'verify' request...");
        set_state(ctx, LIVE_API_VERIFY_WAIT);
    }
}
static void state_hi(LiveAPI *ctx)
{
    char topic[64];
    const uint8_t msg[] = {1};
    snprintf(topic, sizeof(topic),
            "f/%s/hi", ctx->username);
    if(MQTT_client_publish(&ctx->mqtt, topic, msg, sizeof(msg))) {
        ctx->log_debug("live_api: sent 'hi'...");

        set_state(ctx, LIVE_API_IDLE);
    }
}
static void state_idle(LiveAPI *ctx)
{
    LiveAPISendTask *task = &ctx->current_task;
    if(task->type != LIVE_API_TASK_NONE) {
        // while there is something to do, reset the offline interval
        ctx->offline_timestamp = ctx->time_func();

        // 'plain' task: publish it and mark it as 'done'
        if(task->type == LIVE_API_TASK_PLAIN) {

            char topic[64];
            snprintf(topic, sizeof(topic),
                    "f/%s/%s", ctx->username, task->topic);
            uint8_t buffer[LIVE_API_MAX_SEND_LEN];
            const size_t len = live_api_send_queue_get_data(ctx->send_list,
                    task->topic_id, buffer, sizeof(buffer), 0);
            if(MQTT_client_publish(&ctx->mqtt, topic, buffer, len)) {
                live_api_send_queue_task_done(ctx->send_list, task->topic_id);
                task->type = LIVE_API_TASK_NONE;
            } else {
                ctx->log_warning("publish failed");
            }

        // TODO implement support for other types
        } else {
            ctx->log_warning("task type '%d' not supported!", task->type);
            task->type = LIVE_API_TASK_NONE;
        }


    } else {
        const int diff = ctx->time_func() - ctx->offline_timestamp;
        if(diff > OFFLINE_REQUEST_INTERVAL) {
            set_state(ctx, LIVE_API_WANT_OFFLINE);
        }
    }
}

static void state_want_offline(LiveAPI *ctx)
{
    char topic[64];
    const uint8_t msg[] = {0};
    snprintf(topic, sizeof(topic), "f/%s/hi", ctx->username);
    if(MQTT_client_publish(&ctx->mqtt, topic, msg, sizeof(msg))) {
        ctx->log_debug("live_api: sent offline request...");
        set_state(ctx, LIVE_API_WANT_OFFLINE_WAIT);
    }
}

static void state_bye(LiveAPI *ctx)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "f/%s/bye", ctx->username);
    if(MQTT_client_publish(&ctx->mqtt, topic, NULL, 0)) {
        ctx->log_debug("live_api: sent bye...");
        set_state(ctx, LIVE_API_BYE_WAIT);
    }
}

static void state_bye_wait(LiveAPI *ctx)
{
    // if this state is handled, it means the client is idle,
    // so it should have sent the bye message
    MQTT_client_disconnect(&ctx->mqtt);
    set_state(ctx, LIVE_API_NONE);
}

static void state_error(LiveAPI *ctx)
{
    ctx->log_warning("live_api: Error! Something went wrong...");
    MQTT_client_disconnect(&ctx->mqtt);
    set_state(ctx, LIVE_API_NONE);
}




/**
 * Try to get the login credentials from storage.
 *
 * @return      False if it fails: In this case,
 *              ask the server for credentials using the 'register' topic).
 */
static bool get_login_credentials(const LiveAPI *ctx, LoginCredentials *result)
{
    const StorageHAL *storage = &ctx->storage;
    void *storage_ctx = storage->ctx;
    const size_t host_len = strlen(ctx->server_addr);
    char host[host_len];

    bool success = true;

    if(!storage->read(storage_ctx, RESOURCE_HOST, host, sizeof(host))) {
        // no hostname stored
        success = false;
    }
    if(strncmp(ctx->server_addr, host, sizeof(host))) {
        // hostname mismatch
        success = false;
    }

    if(!storage->read(storage_ctx, RESOURCE_PASSWORD,
                result->password, sizeof(result->password))) {
        // no valid password
        success = false;
    }
    if(!storage->read(storage_ctx, RESOURCE_USERNAME,
                result->username, sizeof(result->username))) {
        // no valid username
        success = false;
    }
    if(!success) {
        memset(result, 0, sizeof(LoginCredentials));
    }
    return success;
}

/**
 * Check if we have already 'verified' our credentials.
 * If not, we should publish a 'verify' as soon as possible.
 * This has the effect of locking the account to our clientID and finishing
 * the registration procedure.
 */
static bool is_verified(const LiveAPI *ctx)
{
    const StorageHAL *storage = &ctx->storage;
    void *storage_ctx = storage->ctx;
    char res[64];
    return storage->read(storage_ctx, RESOURCE_VERIFIED,
                res, sizeof(res));
}



static void on_message(void *void_ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    // expect "t/*/<topic>"
    int match = 0;
    while(*topic && match < 3) {
        // unexpected topic: should start with 't'
        if(!match) {
           match++;
           if(*topic != 't') {
            return;
           }
        }
        if(*topic == '/') {
            match++;
        }
        topic++;
    }

    LiveAPI *ctx = (LiveAPI*)void_ctx;

    ctx->log_debug("live_api: got message on topic '%s' (len %u)",
            topic, sizeof_payload);
    if(0 == strcmp(topic, "register")) {
        if(ctx->state == LIVE_API_REGISTER_WAIT) {
            RegisterMessage msg;
            if(!live_api_parse_register(&msg, payload, sizeof_payload)) {
                ctx->log_warning("live_api: parse register response failed!");
                return;
            }
            ctx->log_debug("live_api: register msg parsed!");


            const StorageHAL *storage = &ctx->storage;
            void *storage_ctx = storage->ctx;

            if(!storage->destroy(storage_ctx, RESOURCE_VERIFIED)) {
                ctx->log_warning("live_api: destroying 'verified' failed!");
                return;
            }
            if(!storage->write(storage_ctx, RESOURCE_HOST,
                        ctx->server_addr, strlen(ctx->server_addr))) {
                ctx->log_warning("live_api: saving 'random' failed!");
                return;
            }
            if(!storage->write(storage_ctx, RESOURCE_RANDOM,
                        msg.random, strlen(msg.random))) {
                ctx->log_warning("live_api: saving 'random' failed!");
                return;
            }
            if(!storage->write(storage_ctx, RESOURCE_USERNAME,
                        msg.username, strlen(msg.username))) {
                ctx->log_warning("live_api: saving 'username' failed!");
                return;
            }
            if(!storage->write(storage_ctx, RESOURCE_PASSWORD,
                        msg.password, strlen(msg.password))) {
                ctx->log_warning("live_api: saving 'password' failed!");
                return;
            }

            MQTT_client_disconnect(&ctx->mqtt);
        } 
    } else if(0 == strcmp(topic, "verify")) {
        if(ctx->state == LIVE_API_VERIFY_WAIT) {

            const StorageHAL *storage = &ctx->storage;
            void *storage_ctx = storage->ctx;
            if(!storage->write(storage_ctx, RESOURCE_VERIFIED,
                        "OK", 2)) {
                ctx->log_warning("live_api: saving 'verified' failed!");
                return;
            }
            set_state(ctx, LIVE_API_HI);
        }
    } else if(0 == strcmp(topic, "hi")) {
        if(ctx->state == LIVE_API_WANT_OFFLINE_WAIT) {
            if(sizeof(payload) && (payload[0] == 1)) {
                ctx->log_debug("live_api: offline request allowed");
                set_state(ctx, LIVE_API_BYE);
            } else {
                ctx->log_debug("live_api: offline request denied");
                set_state(ctx, LIVE_API_IDLE);
            }
        }
    }
}

