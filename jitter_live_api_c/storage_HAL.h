#ifndef STORAGE_HAL_H
#define STORAGE_HAL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    RESOURCE_HOST = 1,
    RESOURCE_VERIFIED,
    RESOURCE_USERNAME,
    RESOURCE_PASSWORD,
    RESOURCE_RANDOM,
} ResourceID;

// Note: the resources are guaranteed to be smaller than 64 bytes.

typedef bool (*ReadFunc)(void *void_ctx, ResourceID resource,
        void *data, size_t sizeof_data);
typedef bool (*WriteFunc)(void *void_ctx, ResourceID resource,
        const void *data, size_t sizeof_data);
typedef bool (*DestroyFunc)(void *void_ctx, ResourceID resource);

typedef struct {
    void *ctx;

    ReadFunc read;
    WriteFunc write;
    DestroyFunc destroy;

} StorageHAL;


#endif

