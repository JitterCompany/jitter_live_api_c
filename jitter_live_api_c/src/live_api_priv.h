#ifndef LIVE_API_PRIV_H
#define LIVE_API_PRIV_H


typedef struct {
    char username[LIVE_API_MAX_USERNAME_LEN];
    char password[64];
} LoginCredentials;

#endif

