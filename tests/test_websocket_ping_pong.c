#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void test_client_ping_server_pong(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    const char *data = "ping123";
    websocket_send_ping(client, (const uint8_t *)data, strlen(data));

    uint8_t buf[256];
    size_t len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_PING);

    websocket_send_pong(server, ev.u.ws_message.data, ev.u.ws_message.len);
    len = websocket_get_output(server, buf, sizeof(buf));
    websocket_feed_input(client, buf, len);

    assert(websocket_next_event(client, &ev) == 1);
    assert(ev.type == WS_EVENT_PONG);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_client_ping_server_pong: PASSED\n");
}

static void test_server_ping_client_pong(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    const char *data = "server-ping";
    websocket_send_ping(server, (const uint8_t *)data, strlen(data));

    uint8_t buf[256];
    size_t len = websocket_get_output(server, buf, sizeof(buf));
    websocket_feed_input(client, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(client, &ev) == 1);
    assert(ev.type == WS_EVENT_PING);

    websocket_send_pong(client, ev.u.ws_message.data, ev.u.ws_message.len);
    len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_PONG);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_server_ping_client_pong: PASSED\n");
}

static void test_ping_empty_payload(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    websocket_send_ping(client, NULL, 0);

    uint8_t buf[256];
    size_t len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    protocol_event_t ev;
    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_PING);
    assert(ev.u.ws_message.len == 0);

    websocket_send_pong(server, NULL, 0);
    len = websocket_get_output(server, buf, sizeof(buf));
    websocket_feed_input(client, buf, len);

    assert(websocket_next_event(client, &ev) == 1);
    assert(ev.type == WS_EVENT_PONG);
    assert(ev.u.ws_message.len == 0);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_ping_empty_payload: PASSED\n");
}

static int parse_num(const uint8_t *data, size_t len) {
    char s[32];
    if (len >= sizeof(s)) len = sizeof(s) - 1;
    memcpy(s, data, len);
    s[len] = '\0';
    return atoi(s);
}

static void test_number_exchange_to_42(void)
{
    websocket_ctx_t *client = websocket_create(WS_ROLE_CLIENT);
    websocket_ctx_t *server = websocket_create(WS_ROLE_SERVER);

    uint8_t buf[256];
    protocol_event_t ev;
    char msg[32];
    int num = 0;
    int steps = 0;
    const int MAX_STEPS = 100;

    /* Client starts by sending initial value 0 */
    sprintf(msg, "%d", num);
    websocket_send_text(client, msg);
    size_t len = websocket_get_output(client, buf, sizeof(buf));
    websocket_feed_input(server, buf, len);

    assert(websocket_next_event(server, &ev) == 1);
    assert(ev.type == WS_EVENT_TEXT);
    num = parse_num(ev.u.ws_message.data, ev.u.ws_message.len);
    assert(num == 0);

    while (num < 42 && steps < MAX_STEPS) {
        /* Server side: increment and transmit */
        num++;
        sprintf(msg, "%d", num);
        websocket_send_text(server, msg);
        len = websocket_get_output(server, buf, sizeof(buf));
        websocket_feed_input(client, buf, len);

        assert(websocket_next_event(client, &ev) == 1);
        assert(ev.type == WS_EVENT_TEXT);
        int received = parse_num(ev.u.ws_message.data, ev.u.ws_message.len);
        assert(received == num);

        if (num >= 42) break;

        /* Client side: increment and transmit */
        num++;
        sprintf(msg, "%d", num);
        websocket_send_text(client, msg);
        len = websocket_get_output(client, buf, sizeof(buf));
        websocket_feed_input(server, buf, len);

        assert(websocket_next_event(server, &ev) == 1);
        assert(ev.type == WS_EVENT_TEXT);
        received = parse_num(ev.u.ws_message.data, ev.u.ws_message.len);
        assert(received == num);

        steps++;
    }

    assert(num == 42);

    websocket_destroy(client);
    websocket_destroy(server);
    printf("test_number_exchange_to_42: PASSED\n");
}

int main(void)
{
    test_client_ping_server_pong();
    test_server_ping_client_pong();
    test_ping_empty_payload();
    test_number_exchange_to_42();
    printf("All PING/PONG edge case tests PASSED\n");
    return 0;
}