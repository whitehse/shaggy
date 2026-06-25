#include "http1.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test: Malformed request line should produce error */
static void test_malformed_request_line(void)
{
    http1_ctx_t *ctx = http1_create(HTTP1_ROLE_SERVER);

    const char *bad = "GET\r\n\r\n";
    http1_feed_input(ctx, (const uint8_t *)bad, strlen(bad));

    protocol_event_t ev;
    int got = http1_next_event(ctx, &ev);
    assert(got == 1);
    assert(ev.type == PROTOCOL_EVENT_ERROR);

    http1_destroy(ctx);
    printf("test_malformed_request_line: PASSED\n");
}

/* Test: Malformed status line should produce error */
static void test_malformed_status_line(void)
{
    http1_ctx_t *ctx = http1_create(HTTP1_ROLE_CLIENT);

    const char *bad = "HTTP/1.1\r\n\r\n";
    http1_feed_input(ctx, (const uint8_t *)bad, strlen(bad));

    protocol_event_t ev;
    int got = http1_next_event(ctx, &ev);
    assert(got == 1);
    assert(ev.type == PROTOCOL_EVENT_ERROR);

    http1_destroy(ctx);
    printf("test_malformed_status_line: PASSED\n");
}

/* Test: Line without colon in header section */
static void test_header_without_colon(void)
{
    http1_ctx_t *ctx = http1_create(HTTP1_ROLE_SERVER);

    const char *bad = "GET / HTTP/1.1\r\n"
                      "Host example.com\r\n"
                      "\r\n";
    http1_feed_input(ctx, (const uint8_t *)bad, strlen(bad));

    protocol_event_t ev;
    int found_error = 0;
    while (http1_next_event(ctx, &ev)) {
        if (ev.type == PROTOCOL_EVENT_ERROR) {
            found_error = 1;
            break;
        }
    }
    assert(found_error);

    http1_destroy(ctx);
    printf("test_header_without_colon: PASSED\n");
}

/* Test: Invalid status code (out of range) */
static void test_invalid_status_code(void)
{
    http1_ctx_t *ctx = http1_create(HTTP1_ROLE_CLIENT);

    const char *bad = "HTTP/1.1 999 Invalid\r\n\r\n";
    http1_feed_input(ctx, (const uint8_t *)bad, strlen(bad));

    protocol_event_t ev;
    int got = http1_next_event(ctx, &ev);
    assert(got == 1);
    assert(ev.type == PROTOCOL_EVENT_ERROR);

    http1_destroy(ctx);
    printf("test_invalid_status_code: PASSED\n");
}

int main(void)
{
    test_malformed_request_line();
    test_malformed_status_line();
    test_header_without_colon();
    test_invalid_status_code();
    printf("All HTTP/1.1 Error Handling tests PASSED\n");
    return 0;
}