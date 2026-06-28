#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 * coroutine_http2_kv.c — Illustrative skeleton for coroutine / cooperative multitasking.
 *
 * NOTE: This file will NOT compile or run on a standard Linux host without a
 * coroutine library (e.g. libcoro, or manual ucontext/setjmp). Provided only
 * to document the pattern mandated by ADR 012.
 *
 * Key requirement: the state machine must be yield-friendly and re-entrant.
 * All entry points (feed_input, next_event, get_output) are expected to be
 * safe to call from within a coroutine and to return quickly for yielding.
 */

#include <stdio.h>
#include <string.h>
#include "http2.h"

/* Coroutine primitives would come from the chosen library */
#if defined(CORO_EXAMPLE)
#include "coro.h"   /* placeholder */
#else
#error "This example requires a coroutine runtime (see ADR 012)."
#endif

#define BUF_SIZE 8192

static void http2_coroutine(void *arg) {
    http2_ctx_t *ctx = (http2_ctx_t *)arg;
    uint8_t in[BUF_SIZE], out[BUF_SIZE];

    for (;;) {
        /* yield to scheduler until input is ready */
        /* coro_yield(); */

        size_t n = /* receive from transport */ 0;
        if (n > 0) {
            http2_feed_input(ctx, in, n);
        }

        protocol_event_t ev;
        while (http2_next_event(ctx, &ev) == 1) {
            /* handle event; may yield between events */
        }

        size_t produced = http2_get_output(ctx, out, sizeof(out));
        if (produced > 0) {
            /* send via transport; yield if needed */
        }

        /* coro_yield(); */
    }
}

int main(void) {
    http2_ctx_t *ctx = http2_create_with_role(HTTP2_ROLE_SERVER);

    /* spawn coroutine that drives the http2 state machine */
    /* coro_create(http2_coroutine, ctx); */

    printf("Coroutine + libhttp2 skeleton (ADR 012)\n");
    return 0;
}
