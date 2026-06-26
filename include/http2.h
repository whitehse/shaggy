#ifndef HTTP2_H
#define HTTP2_H

#include "protocol_events.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit states — kept for legacy compatibility */
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

/* Config struct for consistent initialization across modules */
typedef struct {
    int event_queue_size;   /* 0 = use default */
} http2_config_t;

/* Opaque context */
typedef struct http2_ctx http2_ctx_t;

/* === Creation (consistent with http1/websocket) === */
http2_ctx_t *http2_create(http2_role_t role);
http2_ctx_t *http2_create_with_config(http2_role_t role, const http2_config_t *config);

void http2_destroy(http2_ctx_t *ctx);
void http2_reset(http2_ctx_t *ctx);

/* === Core I/O (unchanged) === */
/* feed_input: Accepts any buffer. Returns number of bytes consumed.
   Invalid frames may transition the context to ERROR state.
   Never passes through to syscalls. */
size_t http2_feed_input(http2_ctx_t *ctx, const uint8_t *data, size_t len);

/* get_output: Writes pending frames the library wants to send.
   Returns 0 if nothing to send. */
size_t http2_get_output(http2_ctx_t *ctx, uint8_t *buf, size_t max_len);

/* === New event-driven interface (preferred) === */
/* Returns 0 if an event was written to *event, -1 if no event available.
   When the context is in ERROR state, returns HTTP2_EVENT_ERROR.
   This is the preferred way to consume protocol events. */
int http2_next_event(http2_ctx_t *ctx, protocol_event_t *event);

/* === Legacy getters (kept for compatibility during transition) === */
http2_state_t http2_current_state(const http2_ctx_t *ctx);
uint32_t      http2_error_code(const http2_ctx_t *ctx);
size_t        http2_get_data(http2_ctx_t *ctx, uint8_t *buf, size_t max_len, uint32_t *stream_id);

#ifdef __cplusplus
}
#endif

#endif /* HTTP2_H */