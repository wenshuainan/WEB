#ifndef __SESSION_H__
#define __SESSION_H__


#include "web_server.h"
#include "web_socket_server.h"

#define URI_LENGTH          128
#define HEADER_LENGTH       4096
#define KEY_LENGTH          128
#define VALUE_LENGTH        1024
#define KEY_VALUE_NR        64
#define METHOD_LENGTH       8
#define VERSION_LENGTH      8


typedef enum {
    SESSION_ERR_SUCCESS     = 0,
    SESSION_ERR_PARAM       = -1,
    SESSION_ERR_PARSE       = -2,
    SESSION_ERR_NOTFOUND    = -3,
    SESSION_ERR_RESOURCE    = -4,
    SESSION_ERR_SOCKET      = -5,
}session_err_t;

typedef enum {
    PARSE_LINE,
    PARSE_HEADER,
    PARSE_BODY,
    PARSE_FINISH,
    PARSE_ERR,
}PARSE_STATUS_E;

typedef struct {
    char    method[METHOD_LENGTH];
    char    uri[URI_LENGTH];
    char    version[VERSION_LENGTH];
    int     status_code;
    char    key_value[KEY_VALUE_NR];    /* if method is POST, not parsed now */
    int     method_cursor;
    int     uri_cursor;
    int     switcher;   /* 0=method, 1=uri */
    int     status;
    int     rn_num;
}line_t;

typedef struct {
    char key[KEY_LENGTH];
    char value[VALUE_LENGTH];
    int  key_cursor;
    int  value_cursor;
    int  switcher;  /*0=key, 1=value*/
}key_value_t;

typedef struct {
    key_value_t key_value[KEY_VALUE_NR];
    int         key_value_cursor;
    int         status;
    int         rn_num;
}header_t;

typedef struct {
    unsigned char   *data;
    int             length;
    int             data_cursor;
    int             status;
}body_t;

typedef struct {
    unsigned char   buffer[HEADER_LENGTH];
    int             size;
}buffer_t;

typedef struct {
    web_server_handler_t    web;
    web_session_callback_t  callback;
    pthread_t   tid;
    int         sock;
    int         alive;
    int         parse_status;
    struct {
        line_t      line;
        header_t    header;
        body_t      body;
    }req;
    struct {
        line_t      line;
        header_t    header;
        body_t      body;
        buffer_t    buffer;
    }res;
}session_t;


session_t* session_create(web_server_handler_t web, int sock, web_session_callback_t callback);

int session_destroy(session_t *session);

int session_get_attrs(header_t *header, char *key, char *value, int size);

int session_set_status_code(session_t *session, int code);

int session_set_attr(header_t *header, char *key, char *value);

int session_send(session_t *session, int *new_header, unsigned char *data, int size);

int session_send_web_socket_frame(session_t *session, ws_frame_header_t *header, unsigned char *data, int size);


#endif