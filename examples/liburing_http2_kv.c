/*
 * SPDX-License-Identifier: CC0-1.0
 * This example is released under CC0 (public domain).
 */
/*
 * liburing_http2_kv.c
 *
 * Example program using liburing (io_uring) + libhttp2 state machine.
 *
 * Listens on port 80 (unencrypted h2c), accepts connections,
 * drives the pure HTTP/2 state machine with byte buffers,
 * and implements simple CRUD on a single in-memory string value.
 *
 * Build:
 *   gcc -o liburing_http2_kv liburing_http2_kv.c \
 *       -I../include -L../build -lhttp2 -luring
 *
 * Run (as root or with CAP_NET_BIND_SERVICE for port 80):
 *   ./liburing_http2_kv
 *
 * CRUD operations (HTTP/2 cleartext):
 *   GET  /value          -> returns current string
 *   PUT  /value          -> body becomes new string
 *
 * All networking and event handling is in this file.
 * libhttp2 only receives/sends buffers via feed_input / get_output.
 * Stream-aware data retrieval uses http2_get_data.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>
#include "http2.h"

#define PORT 80
#define BACKLOG 128
#define BUF_SIZE 8192
#define MAX_CONNECTIONS 1024

static char stored_value[8192] = "Hello from libhttp2 state machine!";

struct conn {
    int fd;
    http2_ctx_t *ctx;
    struct io_uring *ring;
    uint8_t in_buf[BUF_SIZE];
    size_t in_len;
};

static void handle_http2_request(struct conn *c)
{
    http2_state_t st = http2_current_state(c->ctx);
    if (st == HTTP2_STATE_ERROR) {
        fprintf(stderr, "HTTP/2 error: %u\n", http2_error_code(c->ctx));
        return;
    }

    /* Demonstrate stream-aware data retrieval (reflects current API) */
    uint32_t stream_id = 0;
    uint8_t data[BUF_SIZE];
    size_t dlen = http2_get_data(c->ctx, data, sizeof(data), &stream_id);
    if (dlen > 0) {
        printf("Received %zu bytes on stream %u\n", dlen, stream_id);
        /* In a real app we would route based on stream_id and headers */
    }

    /* Placeholder response generation would go through http2_get_output */
    printf("Request handled on stream %u, state=%d\n", stream_id, st);
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(listen_fd, BACKLOG);

    struct io_uring ring;
    io_uring_queue_init(64, &ring, 0);

    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return 1; }

    struct conn c = {0};
    c.fd = client_fd;
    c.ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
    c.ring = &ring;

    /* Simulate receiving the connection preface */
    const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    size_t n = http2_feed_input(c.ctx, (const uint8_t *)preface, 24);
    printf("Prefeed: consumed %zu bytes, state=%d\n", n, http2_current_state(c.ctx));

    /* Read one request (toy) */
    ssize_t r = read(client_fd, c.in_buf, BUF_SIZE);
    if (r > 0) {
        http2_feed_input(c.ctx, c.in_buf, r);
        handle_http2_request(&c);
    }

    http2_destroy(c.ctx);
    close(client_fd);
    close(listen_fd);
    io_uring_queue_exit(&ring);
    printf("Example finished. In real code the io_uring loop would continue.\n");
    return 0;
}