/*
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "http2.h"

#define MAX_EVENTS 64
#define BUF_SIZE 8192

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(80), .sin_addr.s_addr = INADDR_ANY };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = listen_fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    uint8_t buf[BUF_SIZE];

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                int client = accept(listen_fd, NULL, NULL);
                http2_ctx_t *ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
                /* store ctx with client fd in real code */
                epoll_ctl(epfd, EPOLL_CTL_ADD, client, &(struct epoll_event){ .events = EPOLLIN, .data.fd = client });
            } else {
                int fd = events[i].data.fd;
                ssize_t r = read(fd, buf, sizeof(buf));
                if (r > 0) {
                    http2_feed_input(/*ctx*/ NULL, buf, r); /* placeholder - real code passes ctx */
                    http2_state_t st = http2_current_state(/*ctx*/ NULL);
                    uint32_t sid = 0;
                    uint8_t data[BUF_SIZE];
                    size_t d = http2_get_data(/*ctx*/ NULL, data, sizeof(data), &sid);
                    (void)st; (void)d; (void)sid;
                }
            }
        }
    }
}