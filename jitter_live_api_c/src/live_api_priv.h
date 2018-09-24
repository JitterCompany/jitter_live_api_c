#ifndef LIVE_API_PRIV_H
#define LIVE_API_PRIV_H


typedef struct {
    uint16_t packet_number;
    uint16_t total_packets;
} FixedDataPacketHeader;

typedef union {
    struct {
        FixedDataPacketHeader header;
        uint8_t data[LIVE_API_FIXED_DATA_PACKET_SIZE];
    };
    uint8_t as_bytes[sizeof(FixedDataPacketHeader) + LIVE_API_FIXED_DATA_PACKET_SIZE];
} FixedDataPacket;

typedef struct {
    char username[LIVE_API_MAX_USERNAME_LEN];
    char password[64];
} LoginCredentials;

#endif

