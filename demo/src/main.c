#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "web_server.h"
#include "web_socket_server.h"

int session_callback(web_session_t *command)
{
    int ret = 0;
    char src_file[128] = {0};

    printf("callback uri: %s\n", command->uri);
    if (strcmp(command->uri, "/") == 0) {
        snprintf(src_file, sizeof(src_file), "%s%s", "resource/test-site", "/index.html");    
    } else {
        snprintf(src_file, sizeof(src_file), "%s%s", "resource/test-site", command->uri);
    }
    printf("src_file=%s\n", src_file);
    ret = access(src_file, F_OK);
    if (ret == 0) {
        FILE *file = fopen(src_file, "r");
        if (file == NULL) {
            return -1;
        }

        web_server_set_session_status_code(command->web, command->session, 200);
        if (strstr(command->uri, ".html")) {
            web_server_set_session_attr(command->web, command->session, "Content-Type", "text/html");
        } else if (strstr(command->uri, ".js")) {
            web_server_set_session_attr(command->web, command->session, "Content-Type", "text/js");
        } else if (strstr(command->uri, ".css")) {
            web_server_set_session_attr(command->web, command->session, "Content-Type", "text/css");
        }
        
        fseek(file, 0, SEEK_END);
        char content_length[16] = {0};
        snprintf(content_length, sizeof(content_length), "%ld", ftell(file));
        web_server_set_session_attr(command->web, command->session, "Content-Length", content_length);
        fseek(file, 0, SEEK_SET);

        unsigned char data[1024] = {0};
        int new_header = 1;
        while ((ret = fread(data, 1, sizeof(data), file)) > 0)
        {
            web_server_send(command->web, command->session, &new_header, data, ret);
        }

        fclose(file);
    }

    return 0;
}

int main()
{
    int ret = 0;

    web_server_handler_t web_handler = NULL;
    web_server_info_t info = {0};
    info.port = 8000;
    info.callback = session_callback;

    ret = web_server_start(&info, &web_handler);
    if (ret != 0) {
        printf("web_server_start %d\n", ret);
        return -1;
    }

    ret = ws_start(9000);
    if (ret != 0) {
        printf("ws start %d\n", ret);
        return -1;
    }

    FILE *yuv = fopen("./resource/test-site/assets/0.yuv", "r");
    if (yuv == NULL) {
        printf("open yuv failed\n");
        return -1;
    }
    unsigned char yuv_data[614400] = {0};
    ret = fread(yuv_data, 1, sizeof(yuv_data), yuv);
    if (ret <= 0) {
        printf("read yuv failed\n");
        return -1;
    }

    while (1)
    {
        if (ws_get_status() == WS_STATUS_OPEN) {
            int sent = ws_send(yuv_data, ret);
            if (sent <= 0) {
                printf("ws send < 0\n");
                return -1;
            }
        }

        sleep(1);
    }
    
    return 0;
}