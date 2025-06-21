#ifndef __WEB_SOCKET_SERVER_H__
#define __WEB_SOCKET_SERVER_H__

#include <stdint.h>

typedef struct ws_frame_header_t{
#if 0
    uint8_t     fin:1;
    uint8_t     rsv:3;
    uint8_t     opcode:4;
    uint8_t     mask:1;
    uint8_t     payload_len:7;
#else
    uint8_t     opcode:4;
    uint8_t     rsv:3;
    uint8_t     fin:1;
    uint8_t     payload_len:7;
    uint8_t     mask:1;
#endif
    uint16_t    extended_payload_length;
}__attribute__ ((packed)) ws_frame_header_t;

typedef enum {
    WS_STATUS_IDLE  = 0,
    WS_STATUS_OPEN  = 1,
}ws_status_t;

int ws_start(int port);

int ws_stop();

int ws_send(unsigned char *data, int size);

int ws_get_status();

#endif