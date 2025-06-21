#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include "session.h"
#include "web_server.h"

#define MAX_SESSION_NR      64


typedef struct {
    int                 status;
    web_server_info_t   info;
    int                 sock;
    pthread_t           tid;
    session_t           *sessions[MAX_SESSION_NR];
}web_server_t;


static void* server_thread(void *arg)
{
    int i;
    int ret;
    web_server_t *server = (web_server_t *)arg;
    int sock = 0;
    struct sockaddr_in addr;
    struct timeval timeout;
    fd_set rset;

    signal(SIGPIPE, SIG_IGN);

    while (server->status == STATUS_WORKING)
    {
		FD_ZERO(&rset);
		FD_SET(server->sock, &rset);

		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

        ret = select(server->sock + 1, &rset, NULL, NULL, &timeout);
        if (ret > 0 && FD_ISSET(server->sock, &rset)) {
            socklen_t addr_len = sizeof(struct sockaddr);
            sock = accept(server->sock, (struct sockaddr *)&addr, &addr_len);
            printf("web server accept a connect (%s:%d)\n", inet_ntoa(addr.sin_addr), addr.sin_port);
            if (sock > 0) {
                for (i = 0; i < MAX_SESSION_NR; i++)
                {
                    if (server->sessions[i] == NULL) {
                        server->sessions[i] = session_create(server, sock, server->info.callback);
                        break;
                    }
                }

                /* resource not enough */
                if (i >= MAX_SESSION_NR) {
                    close(sock);
                }
            }
        }

        for (i = 0; i < MAX_SESSION_NR; i++)
        {
            if (server->sessions[i] && server->sessions[i]->alive == 0) {
                session_destroy(server->sessions[i]);
                server->sessions[i] = NULL;
            }
        }

        usleep(20000);
    }

    return NULL;
}

int web_server_start(web_server_info_t *info, web_server_handler_t *phandler)
{
    int ret = 0;

    if (info == NULL || info->port <= 0 || info->callback == NULL) {
        return WEB_ERR_PARAM;
    }

    if (phandler == NULL) {
        return WEB_ERR_PARAM;
    }

    web_server_t *server = (web_server_t *)malloc(sizeof(web_server_t));
    if (server == NULL) {
        return WEB_ERR_MALLOC;
    } else {
        memset(server, 0, sizeof(web_server_t));
    }

    server->info = *info;

    server->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server->sock < 0) {
        printf("web server start failed, socket %d\n", server->sock);
        return WEB_ERR_SOCK;
    }

    int opt = 1;
    ret = setsockopt(server->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0) {
        printf("web server start failed, setsockopt %d\n", ret);
        close(server->sock);
        return WEB_ERR_SOCK;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	addr.sin_port = htons(server->info.port);
    ret = bind(server->sock, (struct sockaddr *)&addr, sizeof(struct sockaddr));
    if (ret < 0) {
        printf("web server start failed, bind %d, %d\n", ret, errno);
        close(server->sock);
        return WEB_ERR_SOCK;
    }

    ret = listen(server->sock, MAX_SESSION_NR);
    if (ret < 0) {
        printf("web server start failed, listen %d\n", ret);
        close(server->sock);
        return WEB_ERR_SOCK;
    }

    server->status = STATUS_WORKING;
    ret = pthread_create(&server->tid, NULL, server_thread, server);
    if (ret < 0) {
        printf("web server start failed, pthread_create %d\n", ret);
        close(server->sock);
        return WEB_ERR_SOCK;
    }

    if (ret == 0) {
        *phandler = server;
    }

    return ret;
}

int web_server_stop(web_server_handler_t *phandler)
{
    int i;

    if (phandler == NULL) {
        return WEB_ERR_PARAM;
    }

    web_server_t *server = ((web_server_t *)(*phandler));

    if (server->status != STATUS_WORKING) {
        return WEB_ERR_STATUS;
    }

    server->status = STATUS_IDLE;
    pthread_join(server->tid, NULL);
    if (server->sock > 0) {
        close(server->sock);
        server->sock = 0;
    }

    for (i = 0; i < MAX_SESSION_NR; i++)
    {
        if (server->sessions[i] != NULL) {
            session_destroy(server->sessions[i]);
            server->sessions[i] = NULL;
        }
    }

    free(*phandler);
    *phandler = NULL;
    
    return 0;
}

int web_server_get_status(web_server_handler_t handler)
{
    if (handler == NULL) {
        return WEB_ERR_PARAM;
    }

    web_server_t *server = (web_server_t *)handler;

    return server->status;
}

static session_t* get_session(web_server_t *web, web_session_handler_t handler)
{
    session_t *sesn = NULL;
    int i;
    for (i = 0; i < MAX_SESSION_NR; i++)
    {
        if (web->sessions[i] == handler) {
            sesn = web->sessions[i];
            break;
        }
    }
    
    return sesn;
}

int web_server_get_session_attrs(web_server_handler_t web, web_session_handler_t session, char *name, char *value, int size)
{
    if (web == NULL || session == NULL || name == NULL || value == NULL
        || strlen(name) == 0
        ) {
        return WEB_ERR_PARAM;
    }

    session_t *sesn = get_session((web_server_t *)web, session);
    if (sesn == NULL) {
        return WEB_ERR_PARAM;
    }
    

    return session_get_attrs(&sesn->req.header, name, value, size);
}

int web_server_set_session_attr(web_server_handler_t web, web_session_handler_t *session, char *name, char *value)
{
    if (session == NULL || name == NULL || value == NULL
        || strlen(name) == 0 || strlen(value) == 0
        )
    {
        return WEB_ERR_PARAM;
    }

    session_t *sesn = get_session((web_server_t *)web, session);
    if (sesn == NULL) {
        return WEB_ERR_PARAM;
    }

    return session_set_attr(&sesn->res.header, name, value);
}

int web_server_send(web_server_handler_t web, web_session_handler_t *session, int *new_header, unsigned char *data, int size)
{
    if (session == NULL || new_header == NULL) {
        return WEB_ERR_PARAM;
    }

    session_t *sesn = get_session((web_server_t *)web, session);
    if (sesn == NULL) {
        return WEB_ERR_PARAM;
    }

    return session_send(sesn, new_header, data, size);
}

int web_server_set_session_status_code(web_server_handler_t web, web_session_handler_t session, int code)
{
    if (session == NULL) {
        return WEB_ERR_PARAM;
    }

    session_t *sesn = get_session((web_server_t *)web, session);
    if (sesn == NULL) {
        return WEB_ERR_PARAM;
    }

    return session_set_status_code(sesn, code);
}

int web_server_send_ws_frame(web_server_handler_t web, web_session_handler_t *session, ws_frame_header_t *header, unsigned char *data, int size)
{
    if (session == NULL) {
        return WEB_ERR_PARAM;
    }

    session_t *sesn = get_session((web_server_t *)web, session);
    if (sesn == NULL) {
        return WEB_ERR_PARAM;
    }

    return session_send_web_socket_frame(sesn, header, data, size);
}