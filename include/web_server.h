#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#include "web_socket_server.h"

#define URI_SIZE  128

typedef enum {
    WEB_ERR_SUCCESS = 0,
    WEB_ERR_PARAM   = -1,
    WEB_ERR_STATUS  = -2,
    WEB_ERR_SOCK    = -3,
    WEB_ERR_MALLOC  = -4,
    WEB_ERR_PARSE   = -5,
}web_server_err_t;

typedef enum {
    GET,
    POST,
}http_method_t;

typedef enum {
    TEXT_HTML,
    TEXT_JS,
    TEXT_XML,
}http_content_t;

typedef enum {
    STATUS_IDLE,
    STATUS_WORKING,
}WEB_SERVER_STATUS_E;

typedef void* web_server_handler_t;

typedef void* web_session_handler_t;

typedef struct {
    web_server_handler_t    web;
    web_session_handler_t   session;
    char                    uri[URI_SIZE];
    struct {
        unsigned char       *data;
        int                 len;
    }req;
}web_session_t;

typedef int (*web_session_callback_t) (web_session_t *session);

typedef struct {
    int     port;
    web_session_callback_t  callback;
}web_server_info_t;


int web_server_start(web_server_info_t *info, web_server_handler_t *phandler);

int web_server_stop(web_server_handler_t *phandler);

int web_server_get_status(web_server_handler_t handler);

int web_server_get_session_attrs(web_server_handler_t web, web_session_handler_t session, char *name, char *value, int size);

int web_server_set_session_status_code(web_server_handler_t web, web_session_handler_t session, int code);

int web_server_set_session_attr(web_server_handler_t web, web_session_handler_t *session, char *name, char *value);

int web_server_send(web_server_handler_t web, web_session_handler_t *session, int *new_header, unsigned char *data, int size);

int web_server_send_ws_frame(web_server_handler_t web, web_session_handler_t *session, ws_frame_header_t *header, unsigned char *data, int size);

#endif