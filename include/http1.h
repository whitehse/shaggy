#ifndef HTTP1_H
#define HTTP1_H

#include "protocol_events.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP1_ROLE_CLIENT = 0,
    HTTP1_ROLE_SERVER
} http1_role_t;

typedef enum {
    HTTP1_STATE_IDLE = 0,
    HTTP1_STATE_HEADERS,
    HTTP1_STATE_BODY,
    HTTP1_STATE_COMPLETE,
    HTTP1_STATE_ERROR
} http1_state_t;

typedef struct {
    int event_queue_size;
} http1_config_t;

typedef struct http1_ctx http1_ctx_t;

/* Creation */
http1_ctx_t *http1_create(http1_role_t role);
http1_ctx_t *http1_create_with_config(http1_role_t role, const http1_config_t *config);

void http1_destroy(http1_ctx_t *ctx);
void http1_reset(http1_ctx_t *ctx);

/* Feed incoming bytes */
size_t http1_feed_input(http1_ctx_t *ctx, const uint8_t *data, size_t len);

/* Retrieve next event */
int http1_next_event(http1_ctx_t *ctx, protocol_event_t *event);

/* Output generation */
size_t http1_get_output(http1_ctx_t *ctx, uint8_t *buf, size_t max_len);

/* Header helpers */
int http1_get_header(const http1_ctx_t *ctx, const char *name, char *value, size_t max_len);
int http1_set_header(http1_ctx_t *ctx, const char *name, const char *value);

/* Upgrade helpers */
int http1_is_websocket_upgrade(const http1_ctx_t *ctx);
void http1_initiate_websocket_upgrade(http1_ctx_t *ctx, const char *path, const char *host);

#ifdef __cplusplus
}
#endif

#endif /* HTTP1_H */