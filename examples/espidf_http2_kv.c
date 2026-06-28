#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 * espidf_http2_kv.c — Illustrative skeleton for ESP-IDF Event Loop Library integration.
 *
 * NOTE: This file will NOT compile or run on a standard Linux host.
 * It requires the ESP-IDF SDK (esp_event, esp_netif, etc.) and is provided
 * solely to document the integration pattern required by ADR 012.
 *
 * The library remains callback-free; the application registers an esp_event_handler
 * that calls http2_feed_input / http2_next_event / http2_get_output.
 */

#include <stdio.h>
#include <string.h>
#include "http2.h"

/* ESP-IDF specific (not present on host) */
#if defined(ESP_PLATFORM)
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#else
#error "This example requires ESP-IDF headers (ESP_PLATFORM). See ADR 012."
#endif

#define BUF_SIZE 8192

static void http2_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    /* Application-level handler posts data into the http2 state machine */
    http2_ctx_t *ctx = (http2_ctx_t *)arg;
    /* In real code: extract buffer from ESP event data */
    uint8_t *buf = (uint8_t *)data; /* placeholder */
    size_t len = /* ... */ 0;

    http2_feed_input(ctx, buf, len);

    /* Drain events and output via the plumbing API */
    protocol_event_t ev;
    while (http2_next_event(ctx, &ev) == 1) {
        /* caller decides what to do with ev */
    }

    uint8_t out[BUF_SIZE];
    size_t n = http2_get_output(ctx, out, sizeof(out));
    if (n > 0) {
        /* send via esp_netif / lwIP socket or esp_transport */
    }
}

int main(void) {
    /* ESP-IDF init (illustrative) */
    esp_event_loop_create_default();
    http2_ctx_t *ctx = http2_create_with_role(HTTP2_ROLE_SERVER);

    /* Register handler that drives the library */
    esp_event_handler_register(ESP_NETIF_EVENT, ESP_EVENT_ANY_ID,
                               http2_event_handler, ctx);

    /* ... rest of ESP-IDF app (Wi-Fi, server task, etc.) */
    printf("ESP-IDF + libhttp2 skeleton (see ADR 012)\n");
    return 0;
}
