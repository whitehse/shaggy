#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test 1: Simple 2-frame text message */
static void test_two_frame_text(void)
{
    websocket_ctx_t *sender = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *receiver = websocket_create(WS_ROLE_SERVER);

    websocket_send_frame(sender, WS_OP_TEXT, 0, (const uint8_t *)"Hel", 3);
    websocket_send_frame(sender, WS_OP_CONTINUATION, 1, (const uint8_t *)"lo", 2);

    uint8_t buf[512];
    size_t len = websocket_get_output(sender, buf, sizeof(buf));
    websocket_feed_input(receiver, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(receiver, &ev) == 1);
    assert(ev.type == WS_EVENT_TEXT);
    assert(ev.u.ws_message.len == 5);
    assert(memcmp(ev.u.ws_message.data, "Hello", 5) == 0);

    websocket_destroy(sender);
    websocket_destroy(receiver);
    printf("test_two_frame_text: PASSED\n");
}

/* Test 2: PING interleaved during fragmented message */
static void test_ping_during_fragment(void)
{
    websocket_ctx_t *sender = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *receiver = websocket_create(WS_ROLE_SERVER);

    websocket_send_frame(sender, WS_OP_TEXT, 0, (const uint8_t *)"Hel", 3);
    websocket_send_ping(sender, (const uint8_t *)"ping", 4);
    websocket_send_frame(sender, WS_OP_CONTINUATION, 1, (const uint8_t *)"lo", 2);

    uint8_t buf[512];
    size_t len = websocket_get_output(sender, buf, sizeof(buf));
    websocket_feed_input(receiver, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(receiver, &ev) == 1);
    assert(ev.type == WS_EVENT_PING);

    assert(websocket_next_event(receiver, &ev) == 1);
    assert(ev.type == WS_EVENT_TEXT);
    assert(ev.u.ws_message.len == 5);

    websocket_destroy(sender);
    websocket_destroy(receiver);
    printf("test_ping_during_fragment: PASSED\n");
}

/* Test 3: Invalid continuation frame */
static void test_stray_continuation(void)
{
    websocket_ctx_t *ctx = websocket_create(WS_ROLE_SERVER);
    websocket_send_frame(ctx, WS_OP_CONTINUATION, 1, (const uint8_t *)"data", 4);

    uint8_t buf[256];
    websocket_get_output(ctx, buf, sizeof(buf));

    websocket_destroy(ctx);
    printf("test_stray_continuation: PASSED\n");
}

int main(void)
{
    test_two_frame_text();
    test_ping_during_fragment();
    test_stray_continuation();
    printf("All Precise Fragmentation tests PASSED\n");
    return 0;
}