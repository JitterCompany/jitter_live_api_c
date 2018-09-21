#ifndef LIVE_API_SEND_QUEUE_H
#define LIVE_API_SEND_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

// TODO tweak this value: 512 is probably a nice default size?
#ifndef LIVE_API_FIXED_DATA_PACKET_SIZE
    #define LIVE_API_FIXED_DATA_PACKET_SIZE 32
#endif

#define LIVE_API_FIXED_DATA_ACKTEST_SIZE ((7*LIVE_API_FIXED_DATA_PACKET_SIZE)-4)

enum LiveAPITaskType {
    LIVE_API_TASK_NONE = 0,

    LIVE_API_TASK_PLAIN,
    LIVE_API_TASK_FIXED_DATA,
};

typedef struct {
    size_t topic_id;
    const char *topic;
    enum LiveAPITaskType type;
    size_t size; // optional, depending on type
} LiveAPISendTask; 


// the struct is to be defined by the implementation:
// the implementation can use this to store its state
struct LiveAPISendQueue;

typedef struct LiveAPISendQueue LiveAPISendQueue;

// These functions are called by live api
// to process outgoing data that needs to be published.
//
// Note: these should be implemented by the library user.
//

/**
 * Get the next active task
 *
 * @return True if a task is returned via the result parameter
 */
bool live_api_send_queue_get_task(LiveAPISendQueue *ctx,
        LiveAPISendTask *result);

size_t live_api_send_queue_get_data(LiveAPISendQueue *ctx,
        size_t topic_id,
        void *buffer, size_t sizeof_buffer,
        size_t offset);

void live_api_send_queue_task_done(LiveAPISendQueue *ctx,
        size_t topic_id);

#endif

