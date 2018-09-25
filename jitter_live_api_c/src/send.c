#include "send.h"
#include "mqtt.h"
#include "fixed_data.h"
#include "live_api_send_queue.h"

bool send_update_current_task(LiveAPI *ctx)
{
    if(ctx->current_send_task.type != LIVE_API_TASK_NONE) {
        return true;
    }
    if(live_api_send_queue_get_task(ctx->send_list, &ctx->current_send_task)) {
        ctx->fixed_send_state.offset = 0;
        ctx->fixed_send_state.total = fixed_data_calculate_num_packets(
                ctx->current_send_task.size);
        ctx->fixed_send_state.crc = 0;
        return true;
    }
    return false;
}


bool send_handle_incoming(LiveAPI *ctx, const char *topic,
        uint8_t *payload, const size_t sizeof_payload)
{
    return fixed_data_handle_ack(ctx, topic, payload, sizeof_payload);
}

void send(LiveAPI *ctx)
{
    LiveAPISendTask *task = &ctx->current_send_task;
    // even while a fixeddata task is busy, other tasks have more priority
    LiveAPISendTask sub_task;
    if(task->type == LIVE_API_TASK_FIXED_DATA) {
        if(live_api_send_queue_get_task(ctx->send_list, &sub_task)) {
            task = &sub_task;
        }
    }

    // 'plain' task: publish it and mark it as 'done'
    if(task->type == LIVE_API_TASK_PLAIN) {

        uint8_t buffer[LIVE_API_MAX_SEND_LEN];
        const size_t len = live_api_send_queue_get_data(ctx->send_list,
                task->topic_id, buffer, sizeof(buffer), 0);

        if(publish_mqtt(ctx, task->topic, buffer, len)) {
            live_api_send_queue_task_done(ctx->send_list, task->topic_id);
            task->type = LIVE_API_TASK_NONE;
        } else {
            ctx->log_warning("publish failed");
        }


    // 'fixeddata' task: send data in chunks,
    // or send an empty 'announce' chunk if another task is busy
    } else if(task->type == LIVE_API_TASK_FIXED_DATA) {

        fixed_data_send(ctx, task, (task == &sub_task));


    // unknown task
    } else {
        ctx->log_warning("task type '%d' not supported!", task->type);
        task->type = LIVE_API_TASK_NONE;
    }
}
