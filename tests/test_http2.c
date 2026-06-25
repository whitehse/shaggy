#include "http2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void test_preface_and_settings(void)
{
    http2_ctx_t *ctx = http2_create();
    assert(ctx);

    const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    size_t n = http2_feed_input(ctx, (const uint8_t *)preface, 24);
    assert(n == 24);
    assert(http2_current_state(ctx) == HTTP2_STATE_PREFACE);

    // Simulate a SETTINGS frame (length 0, type 4, flags 0, stream 0)
    uint8_t settings[9] = {0,0,0, 4, 0, 0,0,0,0};
    n = http2_feed_input(ctx, settings, 9);
    assert(n == 9);
    assert(http2_current_state(ctx) == HTTP2_STATE_OPEN);

    // Should have produced a SETTINGS ACK
    uint8_t out[64];
    size_t out_len = http2_get_output(ctx, out, sizeof(out));
    assert(out_len == 9);
    assert(out[3] == 4 && out[4] == 1); // SETTINGS + ACK

    http2_destroy(ctx);
    printf("test_preface_and_settings: PASSED\n");
}

static void test_headers_and_data(void)
{
    http2_ctx_t *ctx = http2_create();
    const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    http2_feed_input(ctx, (const uint8_t *)preface, 24);

    // Minimal HEADERS frame on stream 1 (simplified - real HPACK would be more complex)
    uint8_t headers_frame[20] = {
        0,0,1,          // length 1
        1, 0x4,         // HEADERS + END_HEADERS
        0,0,0,1,        // stream 1
        0x88            // indexed :status 200
    };
    http2_feed_input(ctx, headers_frame, 10);

    // DATA frame
    uint8_t data_frame[20] = {
        0,0,5,          // length 5
        0, 0x1,         // DATA + END_STREAM
        0,0,0,1,        // stream 1
        'h','e','l','l','o'
    };
    http2_feed_input(ctx, data_frame, 14);

    uint8_t buf[64];
    uint32_t sid = 0;
    size_t got = http2_get_data(ctx, buf, sizeof(buf), &sid);
    assert(got == 5);
    assert(sid == 1);
    assert(memcmp(buf, "hello", 5) == 0);

    http2_destroy(ctx);
    printf("test_headers_and_data: PASSED\n");
}

static void test_goaway_and_rst(void)
{
    http2_ctx_t *ctx = http2_create();
    const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    http2_feed_input(ctx, (const uint8_t *)preface, 24);

    // GOAWAY
    uint8_t goaway[17] = {
        0,0,8, 7, 0, 0,0,0,0,   // GOAWAY on stream 0
        0,0,0,1,                // last stream 1
        0,0,0,0x2               // PROTOCOL_ERROR
    };
    http2_feed_input(ctx, goaway, 17);
    assert(http2_current_state(ctx) == HTTP2_STATE_CLOSED);
    assert(http2_error_code(ctx) == 0x2);

    http2_destroy(ctx);
    printf("test_goaway_and_rst: PASSED\n");
}

static void test_window_update(void)
{
    http2_ctx_t *ctx = http2_create();
    const char *preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    http2_feed_input(ctx, (const uint8_t *)preface, 24);

    // SETTINGS (length 0) to reach OPEN state (required by the state machine)
    uint8_t settings[9] = {0,0,0, 4, 0, 0,0,0,0};
    http2_feed_input(ctx, settings, 9);
    assert(http2_current_state(ctx) == HTTP2_STATE_OPEN);

    uint8_t wu[13] = {
        0,0,4, 8, 0, 0,0,0,1,   // WINDOW_UPDATE on stream 1
        0,0,0x10,0
    };
    http2_feed_input(ctx, wu, 13);

    // Still OPEN and no crash
    assert(http2_current_state(ctx) == HTTP2_STATE_OPEN);

    http2_destroy(ctx);
    printf("test_window_update: PASSED\n");
}

int main(void)
{
    test_preface_and_settings();
    test_headers_and_data();
    test_goaway_and_rst();
    test_window_update();
    printf("All tests PASSED\n");
    return 0;
}