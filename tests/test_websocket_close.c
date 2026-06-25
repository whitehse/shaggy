#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void test_client_initiated_close(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    websocket_send_close(client, 1000, (const uint8_t *)"Goodbye", 7);
    uint8_t buf[256];
    size_t len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_CLOSE);
    assert(ev.u.ws_close.code == 1000);

    websocket_send_close(server, 1000, NULL, 0);
    len = websocket_get_output(server, buf, sizeof(buf));
    websocket_feed_input(client, buf, len);

    assert(websocket_next_event(client, &ev) == 1);
    assert(ev.type == WS_EVENT_CLOSE);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_client_initiated_close: PASSED\n");
}

static void test_server_initiated_close(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    websocket_send_close(server, 1001, (const uint8_t *)"Going away", 10);
    uint8_t buf[256];
    size_t len = websocket_get_output(server, buf, sizeof(buf));
    websocket_feed_input(client, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(client, &ev) == 1);
    assert(ev.type == WS_EVENT_CLOSE);

    websocket_send_close(client, 1001, NULL, 0);
    len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_CLOSE);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_server_initiated_close: PASSED\n");
}

static void test_close_no_reason(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    websocket_send_close(client, 1000, NULL, 0);
    uint8_t buf[256];
    size_t len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_CLOSE);
    assert(ev.u.ws_close.code == 1000);
    assert(ev.u.ws_close.reason_len == 0);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_close_no_reason: PASSED\n");
}

int main(void)
{
    test_client_initiated_close();
    test_server_initiated_close();
    test_close_no_reason();
    printf("All WebSocket Close handshake tests PASSED\n");
    return 0;
}