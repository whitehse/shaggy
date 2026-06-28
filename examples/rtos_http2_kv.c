#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 * rtos_http2_kv.c — Illustrative skeleton for real-time OS / RT programming.
 *
 * NOTE: This file will NOT compile or run on a standard Linux host.
 * It targets RTOS environments (FreeRTOS, Zephyr, QNX, VxWorks, PREEMPT_RT Linux, etc.)
 * as required by ADR 012. The library must remain suitable for deterministic,
 * bounded-WCET, priority-aware execution.
 *
 * No syscalls, no unbounded operations, re-entrant, bounded stack usage.
 */

#include <stdio.h>
#include <string.h>
#include "http2.h"

/* RTOS headers (platform dependent) */
#if defined(FREERTOS) || defined(ZEPHYR) || defined(__QNX__) || defined(VXWORKS)
#include "rtos_includes.h"  /* placeholder for task, queue, semaphore, etc. */
#else
#error "This example requires an RTOS SDK / real-time headers (see ADR 012)."
#endif

#define BUF_SIZE 8192
#define RTOS_TASK_STACK 4096
#define RTOS_TASK_PRIO  10   /* real-time priority example */

static void http2_rtos_task(void *arg) {
    http2_ctx_t *ctx = (http2_ctx_t *)arg;
    uint8_t in[BUF_SIZE], out[BUF_SIZE];

    for (;;) {
        /* RTOS receive (bounded time) */
        size_t n = /* rtos_recv or queue receive */ 0;
        if (n > 0) {
            http2_feed_input(ctx, in, n);
        }

        protocol_event_t ev;
        while (http2_next_event(ctx, &ev) == 1) {
            /* process; must be deterministic */
        }

        size_t produced = http2_get_output(ctx, out, sizeof(out));
        if (produced > 0) {
            /* rtos_send */
        }

        /* rtos_delay or yield to scheduler respecting priorities */
        /* vTaskDelay / k_sleep / etc. */
    }
}

int main(void) {
    http2_ctx_t *ctx = http2_create_with_role(HTTP2_ROLE_SERVER);

    /* Create RTOS task with real-time priority and bounded stack */
    /* xTaskCreate(http2_rtos_task, "http2", RTOS_TASK_STACK, ctx, RTOS_TASK_PRIO, NULL); */

    printf("RTOS / real-time + libhttp2 skeleton (ADR 012)\n");
    return 0;
}
