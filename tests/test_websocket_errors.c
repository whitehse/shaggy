#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test: Invalid opcode should produce WS_EVENT_ERROR */
static void test_invalid_opcode(void)
{
    websocket_ctx_t *ctx = websocket_create(WS_ROLE_SERVER);

    /* Construct a frame with invalid opcode 0x3 */
    uint8_t bad_frame[10] = { 0x83, 0x00 }; /* FIN + opcode 0x3, no payload */

    websocket_feed_input(ctx, bad_frame, 2);

    protocol_event_t ev;
    int got = websocket_next_event(ctx, &ev);
    assert(got == 1);
    assert(ev.type == WS_EVENT_ERROR);

    websocket_destroy(ctx);
    printf("test_invalid_opcode: PASSED\n");
}

/* Test: RSV bit set should produce error */
static void test_rsv_bit_set(void)
{
    websocket_ctx_t *ctx = websocket_create(WS_ROLE_SERVER);

    uint8_t bad_frame[10] = { 0xC1, 0x00 }; /* RSV1 set + TEXT */

    websocket_feed_input(ctx, bad_frame, 2);

    protocol_event_t ev;
    int got = websocket_next_event(ctx, &ev);
    assert(got == 1);
    assert(ev.type == WS_EVENT_ERROR);

    websocket_destroy(ctx);
    printf("test_rsv_bit_set: PASSED\n");
}

int main(void)
{
    test_invalid_opcode();
    test_rsv_bit_set();
    printf("All WebSocket Error Handling tests PASSED\n");
    return 0;
}