#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 * Updated example demonstrating multi-connection HTTP/2 server with CLI options.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <fcntl.h>
#include "http2.h"

#define MAX_EVENTS 64
#define BUF_SIZE 8192
#define MAX_CONNS 1024

static int listen_port = 8080;
static char listen_addr[64] = "0.0.0.0";
static int enable_tls = 0;
static char key_file[256] = "";
static char cert_file[256] = "";

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--port PORT] [--address ADDR] [--tls] [--key KEYFILE] [--cert CERTFILE]\n", prog);
}

static void parse_args(int argc, char **argv) {
    static struct option long_opts[] = {
        {"port", required_argument, 0, 'p'},
        {"address", required_argument, 0, 'a'},
        {"tls", no_argument, 0, 't'},
        {"key", required_argument, 0, 'k'},
        {"cert", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "p:a:tk:c:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'p': listen_port = atoi(optarg); break;
            case 'a': strncpy(listen_addr, optarg, sizeof(listen_addr)-1); break;
            case 't': enable_tls = 1; break;
            case 'k': strncpy(key_file, optarg, sizeof(key_file)-1); break;
            case 'c': strncpy(cert_file, optarg, sizeof(cert_file)-1); break;
            case 'h': print_usage(argv[0]); exit(0);
            default: print_usage(argv[0]); exit(1);
        }
    }
}

struct conn {
    int fd;
    http2_ctx_t *ctx;
    uint8_t buf[BUF_SIZE];
};

static struct conn conns[MAX_CONNS];
static int num_conns = 0;

static void handle_request(struct conn *c, size_t len) {
    if (memmem(c->buf, len, "index.html", 10)) {
        printf("Serving hello world for /index.html on fd %d\n", c->fd);
        const char *resp = "HTTP/2 200 OK\r\ncontent-length: 13\r\n\r\nhello world\r\n";
        write(c->fd, resp, strlen(resp)); /* demo - real impl uses http2_get_output */
    }
    http2_feed_input(c->ctx, c->buf, len);
    /* multi-conn http2 state handling would continue here */
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    if (enable_tls) {
        printf("kTLS enabled. Key=%s Cert=%s (stub: would do handshake then setsockopt(SOL_TLS, TLS_TX))\n",
               key_file[0] ? key_file : "<none>", cert_file[0] ? cert_file : "<none>");
    }
    printf("Listening on %s:%d (multi-conn epoll example)\n", listen_addr, listen_port);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(listen_port) };
    inet_pton(AF_INET, listen_addr, &addr.sin_addr);
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = listen_fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                int client = accept(listen_fd, NULL, NULL);
                if (client >= 0 && num_conns < MAX_CONNS) {
                    fcntl(client, F_SETFL, O_NONBLOCK);
                    conns[num_conns].fd = client;
                    conns[num_conns].ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
                    struct epoll_event cev = { .events = EPOLLIN, .data.fd = client };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev);
                    num_conns++;
                }
            } else {
                for (int j = 0; j < num_conns; j++) {
                    if (conns[j].fd == events[i].data.fd) {
                        ssize_t r = read(conns[j].fd, conns[j].buf, BUF_SIZE);
                        if (r > 0) {
                            handle_request(&conns[j], r);
                        } else if (r == 0) {
                            close(conns[j].fd);
                            http2_destroy(conns[j].ctx);
                            /* remove from array (simplified) */
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}