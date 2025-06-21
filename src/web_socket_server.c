#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"
#include "sha1.h"
#include "web_server.h"
#include "web_socket_server.h"


#define WS_PORT 9000


static web_server_handler_t s_ws_handle = NULL;
static web_session_handler_t s_ws_session = NULL;
static int s_status = 0;

int ws_session_callback(web_session_t *session)
{
    int ret = 0;
    char Sec_WebSocket_Key[128] = {0};

    ret = web_server_get_session_attrs(s_ws_handle, session->session, "Sec-WebSocket-Key", Sec_WebSocket_Key, sizeof(Sec_WebSocket_Key));
    if (ret != 0 || strlen(Sec_WebSocket_Key) == 0) {
        printf("Web Socket Server parse Sec-WebSocket-Key failed, ret=%d\n", ret);
        return WEB_ERR_PARSE;
    }

    char Sec_WebSocket_Accept_raw[2048] = {0};
    snprintf(Sec_WebSocket_Accept_raw, sizeof(Sec_WebSocket_Accept_raw), "%s%s", Sec_WebSocket_Key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    printf("web server recv key+: %s\n", Sec_WebSocket_Accept_raw);
    unsigned char Sec_WebSocket_Accept_sha1[20] = {0};
    SHA1((char *)Sec_WebSocket_Accept_sha1, Sec_WebSocket_Accept_raw, strlen(Sec_WebSocket_Accept_raw));

    char Sec_WebSocket_Accept_sha1_hex_str[41] = {0};
    int i;
    for (i = 0; i < 20; i++)
    {
        snprintf(Sec_WebSocket_Accept_sha1_hex_str + i * 2, 3, "%02x", Sec_WebSocket_Accept_sha1[i]);
    }
    printf("web server cal sha1: %s\n", Sec_WebSocket_Accept_sha1_hex_str);
    

    char Sec_WebSocket_Accept_base64[128] = {0};
    base64_encode(Sec_WebSocket_Accept_sha1, 20, Sec_WebSocket_Accept_base64);
    printf("web server cal base64: %s\n", Sec_WebSocket_Accept_base64);

    web_server_set_session_status_code(s_ws_handle, session->session, 101);
    web_server_set_session_attr(s_ws_handle, session->session, "Upgrade", "websocket");
    web_server_set_session_attr(s_ws_handle, session->session, "Connection", "Upgrade");
    web_server_set_session_attr(s_ws_handle, session->session, "Sec-WebSocket-Accept", Sec_WebSocket_Accept_base64);

    int new_header = 1;
    web_server_send(s_ws_handle, session->session, &new_header, NULL, 0);

    s_ws_session = session->session;
    s_status = WS_STATUS_OPEN;

    return ret;
}

int ws_start(int port)
{
    web_server_info_t info = {0};
    info.port = WS_PORT;
    info.callback = ws_session_callback;

    return web_server_start(&info, &s_ws_handle);
}

int ws_stop()
{
    s_status = WS_STATUS_IDLE;
    return web_server_stop(&s_ws_handle);
}

int ws_send(unsigned char *data, int size)
{
    int ret = 0;

    if (data == NULL || size <= 0) {
        return WEB_ERR_PARAM;
    }

    if (s_ws_handle != NULL && s_ws_session != NULL) {
        ws_frame_header_t header = {0};
        header.fin = 1;
        header.rsv = 0;
        header.opcode = 0x2;
        header.mask = 0;
        header.payload_len = 126;
        header.extended_payload_length = size;
        ret = web_server_send_ws_frame(s_ws_handle, s_ws_session, &header, data, size);
    }

    if (ret < size) {
        printf("web socket send err, ret: %d size: %d\n", ret, size);
        s_status = WS_STATUS_IDLE;
    }

    return ret;
}

int ws_get_status()
{
    return s_status;
}