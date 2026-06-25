/*
 * SPDX-License-Identifier: CC0-1.0
 */
#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include "http2.h"

static void http2_cb(EV_P_ ev_io *w, int revents) {
    http2_ctx_t *ctx = (http2_ctx_t*)w->data;
    /* read into buffer and feed */
    uint8_t buf[8192];
    /* ... read ... */
    http2_feed_input(ctx, buf, /*len*/0);
    http2_state_t st = http2_current_state(ctx);
    uint32_t sid = 0;
    uint8_t data[8192];
    size_t d = http2_get_data(ctx, data, sizeof(data), &sid);
    (void)st; (void)d; (void)sid;
}

int main(void) {
    struct ev_loop *loop = EV_DEFAULT;
    ev_io http2_watcher;
    /* setup watcher with http2_create_with_role(HTTP2_ROLE_SERVER) */
    ev_io_init(&http2_watcher, http2_cb, /*fd*/0, EV_READ);
    http2_watcher.data = http2_create_with_role(HTTP2_ROLE_SERVER);
    ev_io_start(loop, &http2_watcher);
    ev_run(loop, 0);
    return 0;
}