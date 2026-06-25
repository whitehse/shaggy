#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "protocol_events.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WS_ROLE_CLIENT = 0,
    WS_ROLE_SERVER
} ws_role_t;

typedef enum {
    WS_STATE_IDLE = 0,
    WS_STATE_OPEN,
    WS_STATE_CLOSING,
    WS_STATE_CLOSED,
    WS_STATE_ERROR
} ws_state_t;

typedef enum {
    WS_OP_CONTINUATION = 0x0,
    WS_OP_TEXT         = 0x1,
    WS_OP_BINARY       = 0x2,
    WS_OP_CLOSE        = 0x8,
    WS_OP_PING         = 0x9,
    WS_OP_PONG         = 0xA
} ws_opcode_t;

typedef struct {
    int event_queue_size;
} websocket_config_t;

typedef struct websocket_ctx websocket_ctx_t;

/* Creation */
websocket_ctx_t *websocket_create(ws_role_t role);
websocket_ctx_t *websocket_create_with_config(ws_role_t role, const websocket_config_t *config);

void websocket_destroy(websocket_ctx_t *ctx);
void websocket_reset(websocket_ctx_t *ctx);

/* Feed incoming bytes */
size_t websocket_feed_input(websocket_ctx_t *ctx, const uint8_t *data, size_t len);

/* Retrieve next event */
int websocket_next_event(websocket_ctx_t *ctx, protocol_event_t *event);

/* Get generated frames */
size_t websocket_get_output(websocket_ctx_t *ctx, uint8_t *buf, size_t max_len);

/* High-level send helpers */
int websocket_send_text(websocket_ctx_t *ctx, const char *text);
int websocket_send_binary(websocket_ctx_t *ctx, const uint8_t *data, size_t len);
int websocket_send_ping(websocket_ctx_t *ctx, const uint8_t *data, size_t len);
int websocket_send_pong(websocket_ctx_t *ctx, const uint8_t *data, size_t len);
int websocket_send_close(websocket_ctx_t *ctx, uint16_t code, const uint8_t *reason, size_t reason_len);

/* Low-level frame sending (for advanced testing and fragmentation) */
int websocket_send_frame(websocket_ctx_t *ctx, 
                         ws_opcode_t opcode, 
                         int fin, 
                         const uint8_t *payload, 
                         size_t len);

/* State */
ws_state_t websocket_current_state(const websocket_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_H */