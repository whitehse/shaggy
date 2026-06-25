#include "http1.h"
#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void test_http1_websocket_upgrade_dialectic(void)
{
    http1_ctx_t *http1_client = http1_create(HTTP1_ROLE_CLIENT);
    websocket_ctx_t *ws_client = websocket_create(WS_ROLE_CLIENT);

    http1_initiate_websocket_upgrade(http1_client, "/chat", "example.com");

    uint8_t upgrade_req[512];
    size_t req_len = http1_get_output(http1_client, upgrade_req, sizeof(upgrade_req));

    http1_ctx_t *http1_server = http1_create(HTTP1_ROLE_SERVER);
    websocket_ctx_t *ws_server = websocket_create(WS_ROLE_SERVER);

    http1_feed_input(http1_server, upgrade_req, req_len);
    assert(http1_is_websocket_upgrade(http1_server));

    const char *resp = "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\nConnection: Upgrade\r\n\r\n";
    http1_feed_input(http1_client, (const uint8_t *)resp, strlen(resp));

    websocket_send_text(ws_client, "Hello via common events!");

    uint8_t frame[256];
    size_t flen = websocket_get_output(ws_client, frame, sizeof(frame));

    websocket_feed_input(ws_server, frame, flen);

    protocol_event_t ev;
    int got = websocket_next_event(ws_server, &ev);
    assert(got);
    assert(ev.type == WS_EVENT_TEXT);
    assert(ev.u.ws_message.len == strlen("Hello via common events!"));

    websocket_send_text(ws_server, "Reply via common events");
    flen = websocket_get_output(ws_server, frame, sizeof(frame));
    websocket_feed_input(ws_client, frame, flen);

    got = websocket_next_event(ws_client, &ev);
    assert(got);
    assert(ev.type == WS_EVENT_TEXT);

    http1_destroy(http1_client);
    http1_destroy(http1_server);
    websocket_destroy(ws_client);
    websocket_destroy(ws_server);

    printf("test_http1_websocket_upgrade_dialectic (unified events): PASSED\n");
}

int main(void)
{
    test_http1_websocket_upgrade_dialectic();
    printf("All tests PASSED\n");
    return 0;
}