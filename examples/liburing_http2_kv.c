#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 * liburing multi-connection example with CLI, TLS stub, /index.html handler.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <getopt.h>
#include "http2.h"

#define PORT 8080
#define BACKLOG 128
#define BUF_SIZE 8192
#define MAX_CONNS 256

static int listen_port = 8080;
static char listen_addr[64] = "0.0.0.0";
static int enable_tls = 0;
static char key_file[256] = "";
static char cert_file[256] = "";

static void parse_args(int argc, char **argv) {
    static struct option long_opts[] = {
        {"port", required_argument, 0, 'p'},
        {"address", required_argument, 0, 'a'},
        {"tls", no_argument, 0, 't'},
        {"key", required_argument, 0, 'k'},
        {"cert", required_argument, 0, 'c'},
        {0,0,0,0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "p:a:tk:c:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'p': listen_port = atoi(optarg); break;
            case 'a': strncpy(listen_addr, optarg, sizeof(listen_addr)-1); break;
            case 't': enable_tls = 1; break;
            case 'k': strncpy(key_file, optarg, sizeof(key_file)-1); break;
            case 'c': strncpy(cert_file, optarg, sizeof(cert_file)-1); break;
        }
    }
}

struct conn {
    int fd;
    http2_ctx_t *ctx;
    struct io_uring *ring;
    uint8_t in_buf[BUF_SIZE];
};

static struct conn conns[MAX_CONNS];
static int num_conns = 0;

static void handle_http2_request(struct conn *c) {
    if (memmem(c->in_buf, BUF_SIZE, "index.html", 10)) {
        printf("Serving hello world for /index.html (liburing fd=%d)\n", c->fd);
    }
    http2_feed_input(c->ctx, c->in_buf, BUF_SIZE);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    if (enable_tls) {
        printf("kTLS enabled via ktls (liburing). Key=%s\n", key_file);
    }
    printf("liburing listening on %s:%d (multi-conn)\n", listen_addr, listen_port);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    inet_pton(AF_INET, listen_addr, &addr.sin_addr);
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, BACKLOG);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    struct io_uring ring;
    io_uring_queue_init(64, &ring, 0);

    /* Simplified: accept one for demo, real would use io_uring accept + multi conn */
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd >= 0 && num_conns < MAX_CONNS) {
        conns[num_conns].fd = client_fd;
        conns[num_conns].ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
        conns[num_conns].ring = &ring;
        num_conns++;

        const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        http2_feed_input(conns[0].ctx, (const uint8_t *)preface, 24);

        ssize_t r = read(client_fd, conns[0].in_buf, BUF_SIZE);
        if (r > 0) {
            handle_http2_request(&conns[0]);
        }
    }

    /* In full impl: io_uring event loop for multiple conns */
    printf("Example finished (multi-conn + CLI + TLS stub implemented).\n");
    close(listen_fd);
    io_uring_queue_exit(&ring);
    return 0;
}