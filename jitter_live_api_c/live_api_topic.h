#ifndef LIVE_API_TOPIC_H
#define LIVE_API_TOPIC_H


enum LiveAPITaskType {
    LIVE_API_TASK_NONE = 0,

    LIVE_API_TASK_PLAIN,
    LIVE_API_TASK_FIXED_DATA,
};

typedef struct {
    const char *topic;
    enum LiveAPITaskType type;
} LiveAPITopic; 



#endif

