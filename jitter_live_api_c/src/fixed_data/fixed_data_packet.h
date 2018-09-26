#ifndef FIXED_DATA_PACKET_H
#define FIXED_DATA_PACKET_H

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

#endif

