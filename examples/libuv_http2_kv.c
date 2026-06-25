/*
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include "http2.h"

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        http2_ctx_t *ctx = (http2_ctx_t*)stream->data;
        http2_feed_input(ctx, (const uint8_t*)buf->base, nread);
        http2_state_t st = http2_current_state(ctx);
        uint32_t sid = 0;
        uint8_t data[8192];
        size_t dlen = http2_get_data(ctx, data, sizeof(data), &sid);
        (void)st; (void)dlen; (void)sid;
    }
}

int main(void) {
    uv_loop_t *loop = uv_default_loop();
    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", 80, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    uv_listen((uv_stream_t*)&server, 128, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}