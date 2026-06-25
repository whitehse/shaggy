#ifndef HTTP2_H
#define HTTP2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit states — libassh-inspired deterministic machine */
typedef enum {
    HTTP2_STATE_IDLE = 0,
    HTTP2_STATE_PREFACE,
    HTTP2_STATE_SETTINGS,
    HTTP2_STATE_OPEN,
    HTTP2_STATE_HALF_CLOSED,
    HTTP2_STATE_CLOSED,
    HTTP2_STATE_ERROR
} http2_state_t;

/* Connection role for dialectic client/server support (ADR 004) */
typedef enum {
    HTTP2_ROLE_CLIENT = 0,
    HTTP2_ROLE_SERVER
} http2_role_t;

/* Opaque context — caller never inspects internals */
typedef struct http2_ctx http2_ctx_t;

/* Public API — pure functions, no syscalls, no callbacks */
http2_ctx_t *http2_create(void);
http2_ctx_t *http2_create_with_role(http2_role_t role);
void http2_destroy(http2_ctx_t *ctx);

void http2_reset(http2_ctx_t *ctx);

/* Feed input bytes from caller (io_uring completion, etc.) */
size_t http2_feed_input(http2_ctx_t *ctx, const uint8_t *data, size_t len);

/* Query current state */
http2_state_t http2_current_state(const http2_ctx_t *ctx);

/* Retrieve error code if in ERROR state */
uint32_t http2_error_code(const http2_ctx_t *ctx);

/* Request output bytes (e.g. SETTINGS ACK, WINDOW_UPDATE, etc.) */
size_t http2_get_output(http2_ctx_t *ctx, uint8_t *buf, size_t max_len);

/* Retrieve received DATA payload (for application consumption) */
size_t http2_get_data(http2_ctx_t *ctx, uint8_t *buf, size_t max_len, uint32_t *stream_id);

#ifdef __cplusplus
}
#endif

#endif /* HTTP2_H */