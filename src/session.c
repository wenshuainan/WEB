#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "session.h"

#define MAX_CONTENT_LENGTH  1048576 /* 1M */

#define HTTP_VERSION        "HTTP/1.1"


static int get_content_length(session_t *session)
{
    int i;
    for (i = 0; i < session->req.header.key_value_cursor; i++)
    {
        if (strcasecmp("content-length", session->req.header.key_value[i].key) == 0) {
            return atoi(session->req.header.key_value[i].value);
        }
    }

    return SESSION_ERR_NOTFOUND;
}

static int parse_line(line_t *line, unsigned char *data, int len)
{
    int ret_parsed = 0;
    int i;

    for (i = 0; i < len; i++, ret_parsed++)
    {
        if (data[i] == ' ') {
            line->switcher++;
            line->rn_num = 0;
            continue;
        } else if (data[i] == '\r') {
            line->rn_num++;
            continue;
        } else if (data[i] == '\n') {
            line->rn_num++;
            if (line->rn_num == 2) {
                ret_parsed++;
                line->status = 1;
                break;
            }
        }

        line->rn_num = 0;
        if (line->switcher == 0) {
            if (line->method_cursor >= sizeof(line->method) - 1) {
                ret_parsed = -1;
                break;
            }
            line->method[line->method_cursor++] = data[i];
        } else if (line->switcher == 1) {
            if (line->uri_cursor >= sizeof(line->uri) - 1) {
                ret_parsed = -1;
                break;
            }
            line->uri[line->uri_cursor++] = data[i];
        } else {
            continue;
        }
    }

    return ret_parsed;
}

static int parse_header(header_t *header, unsigned char *data, int len)
{
    int ret_parsed = 0;
    int i;
    key_value_t *key_value = &header->key_value[header->key_value_cursor];

    for (i = 0; i < len; i++, ret_parsed++)
    {
        if (data[i] == ':') {
            if (key_value->switcher == 0) {
                key_value->switcher = 1;
                header->rn_num = 0;
                continue;
            }
        } else if (data[i] == ' ' || data[i] == '\t') {
            header->rn_num = 0;
            continue;
        } else if (data[i] == '\r') {
            header->rn_num++;
            continue;
        } else if (data[i] == '\n') {
            header->rn_num++;
            if (header->rn_num == 2) {
                if (header->key_value_cursor >= KEY_VALUE_NR - 1) {
                    printf("parse_header error, key_value_cursor=%d, KEY_VALUE_NR=%d\n", header->key_value_cursor, KEY_VALUE_NR);
                    ret_parsed = -1;
                    break;
                } else {
                    header->key_value_cursor++;
                    key_value = &header->key_value[header->key_value_cursor];
                    continue;
                }
            } else if (header->rn_num == 4) {
                ret_parsed++;
                header->status = 1;
                key_value->key_cursor--;
                break;
            }
        }

        header->rn_num = 0;
        if (key_value->switcher == 0) {
            if (key_value->key_cursor >= sizeof(key_value->key) - 1) {
                ret_parsed = -1;
                break;
            }
            key_value->key[key_value->key_cursor++] = data[i];
        } else {
            if (key_value->value_cursor >= sizeof(key_value->value) - 1) {
                ret_parsed = -1;
                break;
            }
            key_value->value[key_value->value_cursor++] = data[i];
        }
    }
    
    return ret_parsed;
}

static int parse_body(body_t *body, unsigned char *data, int len)
{
    int ret_parsed = body->length - body->data_cursor;

    ret_parsed = len > ret_parsed ? ret_parsed : len;
    memcpy(body->data, data, ret_parsed);
    body->data_cursor += ret_parsed;
    if (body->data_cursor == body->length) {
        body->status = 1;
    }

    return ret_parsed;
}

static int parse_recv(session_t *session, unsigned char *recv_data, int recv_len)
{
    int ret_parsed = 0;

    do
    {
        int tmp = 0;
        switch (session->parse_status)
        {
        case PARSE_LINE:
            tmp = parse_line(&session->req.line, recv_data + ret_parsed, recv_len - ret_parsed);
            if (tmp < 0) {
                session->parse_status = PARSE_ERR;
                break;
            }
            if (session->req.line.status == 1) {
                session->parse_status = PARSE_HEADER;
            }
            ret_parsed += tmp;
            break;
        case PARSE_HEADER:
            tmp = parse_header(&session->req.header, recv_data + ret_parsed, recv_len - ret_parsed);
            if (tmp < 0) {
                session->parse_status = PARSE_ERR;
                break;
            }
            if (session->req.header.status == 1) {
                int content_length = get_content_length(session);
                if (content_length <= 0) {
                    session->parse_status = PARSE_FINISH;
                    break;
                } else if (content_length > MAX_CONTENT_LENGTH) {
                    session->parse_status = PARSE_ERR;
                    break;
                }
                session->req.body.data = (unsigned char *)malloc(content_length);
                if (session->req.body.data == NULL) {
                    session->parse_status = PARSE_ERR;
                    break;
                } else {
                    memset(session->req.body.data, 0, content_length);
                    session->req.body.length = content_length;
                    session->parse_status = PARSE_BODY;
                }
            }
            ret_parsed += tmp;
            break;
        case PARSE_BODY:
            tmp = parse_body(&session->req.body, recv_data + ret_parsed, recv_len - ret_parsed);
            if (tmp < 0) {
                session->parse_status = PARSE_ERR;
                break;
            }
            if (session->req.body.status == 1) {
                session->parse_status = PARSE_FINISH;
            }
            ret_parsed += tmp;
            break;
        default:
            break;
        }

        if (session->parse_status == PARSE_ERR) {
            break;
        }
    } while (session->parse_status != PARSE_FINISH && ret_parsed < recv_len);
    
    return ret_parsed;
}

static int session_reset(session_t *session)
{
    session->parse_status = PARSE_LINE;
    memset(&session->req.line, 0, sizeof(session->req.line));
    memset(&session->req.header, 0, sizeof(session->req.header));
    if (session->req.body.data != NULL) {
        free(session->req.body.data);
        session->req.body.data = NULL;
    }
    memset(&session->req.body, 0, sizeof(session->req.body));
    memset(&session->res, 0, sizeof(session->res));

    return 0;
}

static void session_dump(session_t *session)
{
    int i;
    printf("************************************dump************************************\n");
    printf("************************************line\n");
    printf("method=%s\n", session->req.line.method);
    printf("uri=%s\n", session->req.line.uri);
    printf("************************************header\n");
    for (i = 0; i < session->req.header.key_value_cursor; i++)
    {
        printf("key=%s\t", session->req.header.key_value[i].key);
        printf("value=%s\n", session->req.header.key_value[i].value);
    }

    return;
}

int session_get_attrs(header_t *header, char *key, char *value, int size)
{
    if (header == NULL || key == NULL || value == NULL
        || strlen(key) == 0 || size < 0
        ) {
        return SESSION_ERR_PARAM;
    }

    int i;
    for (i = 0; i < header->key_value_cursor; i++)
    {
        if (strcmp(key, header->key_value[i].key) == 0) {
            strncpy(value, header->key_value[i].value, size);
            return 0;
        }
    }
    
    return SESSION_ERR_NOTFOUND;
}

int session_set_attr(header_t *header, char *key, char *value)
{
    if (header == NULL || key == NULL || value == NULL
        || strlen(key) == 0 || strlen(value) == 0
        ) {
        return SESSION_ERR_PARAM;
    }

    strncpy(header->key_value[header->key_value_cursor].key, key, 
            sizeof(header->key_value[header->key_value_cursor].key)
            );

    strncpy(header->key_value[header->key_value_cursor].value,
            value, sizeof(header->key_value[header->key_value_cursor].value)
            );

    header->key_value_cursor++;

    return 0;
}

static int construct_line(buffer_t *send_buffer, line_t *line)
{
    if (send_buffer->size + strlen(HTTP_VERSION) > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        strncpy((char *)send_buffer->buffer + send_buffer->size, HTTP_VERSION, strlen(HTTP_VERSION));
        send_buffer->size += strlen(HTTP_VERSION);
    }
    if (send_buffer->size + 1 > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        send_buffer->buffer[send_buffer->size++] = ' ';
    }

    char status_code_str[16] = {0};
    snprintf(status_code_str, sizeof(status_code_str), "%d", line->status_code);
    if (send_buffer->size + strlen(status_code_str) > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        strncpy((char *)send_buffer->buffer + send_buffer->size, status_code_str, strlen(status_code_str));
        send_buffer->size += strlen(status_code_str);
    }
    if (send_buffer->size + 1 > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        send_buffer->buffer[send_buffer->size++] = ' ';
    }

    char *status_des = NULL;
    switch (line->status_code)
    {
    case 101:
        status_des = "Switching Protocols";
        break;
    case 200:
        status_des = "OK";
        break;
    case 404:
        status_des = "Not Found";
        break;
    case 500:
        status_des = "Internal Server Error";
        break;
    default:
        status_des = "Internal Server Error";
        break;
    }
    if (send_buffer->size + strlen(status_des) > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        strncpy((char *)send_buffer->buffer + send_buffer->size, status_des, strlen(status_des));
        send_buffer->size += strlen(status_des);
    }

    if (send_buffer->size + 2 > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        send_buffer->buffer[send_buffer->size++] = '\r';
        send_buffer->buffer[send_buffer->size++] = '\n';
    }

    return 0;
}

static int construct_header(buffer_t *send_buffer, header_t *header)
{
    int i;
    for (i = 0; i < header->key_value_cursor; i++)
    {
        if (send_buffer->size + strlen(header->key_value[i].key) > sizeof(send_buffer->buffer)) {
            return SESSION_ERR_RESOURCE;
        } else {
            strncpy((char *)send_buffer->buffer + send_buffer->size, header->key_value[i].key, strlen(header->key_value[i].key));
            send_buffer->size += strlen(header->key_value[i].key);
        }

        if (send_buffer->size + 2 > sizeof(send_buffer->buffer)) {
            return SESSION_ERR_RESOURCE;
        } else {
            send_buffer->buffer[send_buffer->size++] = ':';
            send_buffer->buffer[send_buffer->size++] = ' ';
        }

        if (send_buffer->size + strlen(header->key_value[i].value) > sizeof(send_buffer->buffer)) {
            return SESSION_ERR_RESOURCE;
        } else {
            strncpy((char *)send_buffer->buffer + send_buffer->size, header->key_value[i].value, strlen(header->key_value[i].value));
            send_buffer->size += strlen(header->key_value[i].value);
        }

        if (send_buffer->size + 2 > sizeof(send_buffer->buffer)) {
            return SESSION_ERR_RESOURCE;
        } else {
            send_buffer->buffer[send_buffer->size++] = '\r';
            send_buffer->buffer[send_buffer->size++] = '\n';
        }
    }

    if (send_buffer->size + 2 > sizeof(send_buffer->buffer)) {
        return SESSION_ERR_RESOURCE;
    } else {
        send_buffer->buffer[send_buffer->size++] = '\r';
        send_buffer->buffer[send_buffer->size++] = '\n';
    }

    return 0;    
}

static int session_handle_request(session_t *session)
{
    if (session->callback == NULL) {
        return SESSION_ERR_PARAM;
    }

    web_session_t command = {0};
    command.web = session->web;
    command.session = session;
    strncpy(command.uri, session->req.line.uri, sizeof(command.uri));
    command.req.data = session->req.body.data;
    command.req.len = session->req.body.length;

    return session->callback(&command);
}

static void* session_thread(void *arg)
{
    session_t *session = (session_t *)arg;
    unsigned char recv_buf[4096] = {0};

    signal(SIGPIPE, SIG_IGN);

    while (session->alive)
    {
        int size = recv(session->sock, recv_buf, sizeof(recv_buf), 0);
        if (size <= 0) {
            printf("session recv err %d\n", size);
            break;
        }

        size = parse_recv(session, recv_buf, size);
        if (size < 0) {
            printf("session parse recv err %d\n", size);
            break;
        }

        if (session->parse_status == PARSE_ERR) {
            printf("session parse err\n");
            break;
        } else if (session->parse_status == PARSE_FINISH) {
            /* session_dump(session); */
            int ret = session_handle_request(session);
            if (ret != 0) {
                printf("session handle request failed %d\n", ret);
                break;
            }
            
            session_reset(session);
        }

        usleep(200);
    }

    session->alive = 0;
    session_reset(session);

    return NULL;
}

session_t* session_create(web_server_handler_t web, int sock, web_session_callback_t callback)
{
    int ret = 0;

    session_t *session = (session_t *)malloc(sizeof(session_t));
    if (session != NULL) {
        memset(session, 0, sizeof(session_t));
    } else {
        return NULL;
    }
    session->web = web;
    session->callback = callback;
    session->sock = sock;
    session->alive = 1;
    ret = pthread_create(&session->tid, NULL, session_thread, session);
    if (ret < 0) {
        free(session);
        printf("session create failed, thread create error\n");
    }

    printf("web session create %d\n", sock);

    return session;
}

int session_destroy(session_t *session)
{
    int ret = 0;

    printf("web session destroy %d\n", session->sock);

    close(session->sock);
    session->alive = 0;
    pthread_join(session->tid, NULL);
    free(session);

    return ret;
}

int session_set_status_code(session_t *session, int code)
{
    session->res.line.status_code = code;
    return 0;
}

int session_send(session_t *session, int *new_header, unsigned char *data, int size)
{
    int ret = 0;

    if (*new_header) {
        ret = construct_line(&session->res.buffer, &session->res.line);
        if (ret != 0) {
            printf("construct line failed %d\n", ret);
            return ret;
        }
        ret = construct_header(&session->res.buffer, &session->res.header);
        if (ret != 0) {
            printf("construct header failed %d\n", ret);
            return ret;
        }
        ret = send(session->sock, session->res.buffer.buffer, session->res.buffer.size, 0);
        if (ret < 0) {
            printf("session send failed\n");
            return ret;
        }

        *new_header = 0;
    }

    if (data != NULL && size > 0) {
        ret = send(session->sock, data, size, 0);
    }

    return ret;
}

int session_send_web_socket_frame(session_t *session, ws_frame_header_t *header, unsigned char *data, int size)
{
    int unit_size = 1400;
    int sent_size = 0;
    while (size > 0)
    {
        int send_size = 0;
        if (unit_size < size) {
            send_size = unit_size;
            header->fin = 0;
        } else {
            send_size = size;
            header->fin = 1;
        }
        header->extended_payload_length = htons(send_size);
        size -= send_size;

        struct iovec iov[2] = {{0}};
        iov[0].iov_base = header;
        iov[0].iov_len = sizeof(ws_frame_header_t);
        iov[1].iov_base = data + sent_size;
        iov[1].iov_len = send_size;

        int sent = writev(session->sock, iov, 2);
        if (sent <= 0) {
            printf("session writev err\n");
            break;
        }
        sent_size += sent;
        header->opcode = 0x0;
    }

    return sent_size;
}