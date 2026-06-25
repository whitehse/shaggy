#ifndef PROTOCOL_EVENTS_H
#define PROTOCOL_EVENTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROTOCOL_NONE = 0,
    PROTOCOL_HTTP1,
    PROTOCOL_WEBSOCKET,
} protocol_type_t;

typedef enum {
    PROTOCOL_EVENT_NONE = 0,
    PROTOCOL_EVENT_ERROR,

    HTTP1_EVENT_REQUEST_LINE,
    HTTP1_EVENT_STATUS_LINE,
    HTTP1_EVENT_HEADER,
    HTTP1_EVENT_HEADERS_COMPLETE,
    HTTP1_EVENT_BODY_CHUNK,
    HTTP1_EVENT_UPGRADE_DETECTED,

    WS_EVENT_TEXT,
    WS_EVENT_BINARY,
    WS_EVENT_PING,
    WS_EVENT_PONG,
    WS_EVENT_CLOSE,
    WS_EVENT_ERROR,
} protocol_event_type_t;

typedef struct {
    char method[16];
    char path[256];
    char version[16];
} http1_request_line_t;

typedef struct {
    char version[16];
    int  status;
    char reason[64];
} http1_status_line_t;

typedef struct {
    char name[64];
    char value[256];
} http1_header_t;

typedef struct {
    const uint8_t *data;
    size_t         len;
} protocol_body_chunk_t;

typedef struct {
    uint16_t       code;
    const uint8_t *reason;
    size_t         reason_len;
} ws_close_t;

typedef struct {
    const uint8_t *data;
    size_t         len;
} ws_message_t;

typedef struct {
    protocol_type_t      protocol;
    protocol_event_type_t type;

    union {
        http1_request_line_t  http1_request;
        http1_status_line_t   http1_status;
        http1_header_t        http1_header;
        protocol_body_chunk_t body;

        ws_message_t          ws_message;
        ws_close_t            ws_close;
    } u;
} protocol_event_t;

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_EVENTS_H */