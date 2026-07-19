#define _GNU_SOURCE
/*
 * http1.c — Minimal HTTP/1.1 PDU plumbing (ADR 006)
 */
#include "http1.h"
#include "protocol_events.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_HEADERS      32
#define MAX_HEADER_NAME  64
#define MAX_HEADER_VALUE 256
#define MAX_OUT_BUF      8192
#define LINE_BUF_SIZE    512
#define HTTP1_DEFAULT_EVENT_QUEUE_SIZE 8

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} http1_hdr_t;

struct http1_ctx {
    http1_role_t   role;
    http1_state_t  state;

    http1_hdr_t    headers[MAX_HEADERS];
    int            header_count;

    uint8_t        out_buf[MAX_OUT_BUF];
    size_t         out_len;
    size_t         out_sent;

    char           method[16];
    char           path[256];
    char           version[16];
    int            status_code;
    char           reason[64];
    int            is_upgrade;

    char           line[LINE_BUF_SIZE];
    size_t         line_pos;

    /* Body handling */
    long           content_length;
    long           body_bytes_received;
    int            content_length_seen;

    /* Event queue */
    protocol_event_t *event_queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    int queue_count;
};

static void enqueue_event(http1_ctx_t *ctx, const protocol_event_t *ev)
{
    if (ctx->queue_count >= ctx->queue_size) return;
    ctx->event_queue[ctx->queue_tail] = *ev;
    ctx->queue_tail = (ctx->queue_tail + 1) % ctx->queue_size;
    ctx->queue_count++;
}

http1_ctx_t *http1_create(http1_role_t role)
{
    http1_config_t default_config = { .event_queue_size = HTTP1_DEFAULT_EVENT_QUEUE_SIZE };
    return http1_create_with_config(role, &default_config);
}

http1_ctx_t *http1_create_with_config(http1_role_t role, const http1_config_t *config)
{
    if (!config || config->event_queue_size <= 0) return NULL;

    http1_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->event_queue = calloc(config->event_queue_size, sizeof(protocol_event_t));
    if (!ctx->event_queue) {
        free(ctx);
        return NULL;
    }

    ctx->role = role;
    ctx->queue_size = config->event_queue_size;
    http1_reset(ctx);
    return ctx;
}

void http1_destroy(http1_ctx_t *ctx)
{
    if (ctx) {
        free(ctx->event_queue);
        free(ctx);
    }
}

void http1_reset(http1_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->state = HTTP1_STATE_IDLE;
    ctx->header_count = 0;
    ctx->out_len = 0;
    ctx->out_sent = 0;
    ctx->is_upgrade = 0;
    ctx->line_pos = 0;
    ctx->content_length = -1;
    ctx->body_bytes_received = 0;
    ctx->content_length_seen = 0;
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_count = 0;
    memset(ctx->method, 0, sizeof(ctx->method));
    memset(ctx->path, 0, sizeof(ctx->path));
    memset(ctx->version, 0, sizeof(ctx->version));
    memset(ctx->line, 0, sizeof(ctx->line));
}

static void emit_event(http1_ctx_t *ctx, protocol_event_type_t type)
{
    if (ctx->queue_count >= ctx->queue_size) return;

    protocol_event_t ev;
    ev.protocol = PROTOCOL_HTTP1;
    ev.type = type;

    switch (type) {
    case HTTP1_EVENT_REQUEST_LINE:
        strncpy(ev.u.http1_request.method, ctx->method, sizeof(ev.u.http1_request.method));
        strncpy(ev.u.http1_request.path,   ctx->path,   sizeof(ev.u.http1_request.path));
        strncpy(ev.u.http1_request.version, ctx->version, sizeof(ev.u.http1_request.version));
        break;

    case HTTP1_EVENT_STATUS_LINE:
        strncpy(ev.u.http1_status.version, ctx->version, sizeof(ev.u.http1_status.version));
        ev.u.http1_status.status = ctx->status_code;
        strncpy(ev.u.http1_status.reason, ctx->reason, sizeof(ev.u.http1_status.reason));
        break;

    case HTTP1_EVENT_UPGRADE_DETECTED:
        break;

    default:
        break;
    }

    enqueue_event(ctx, &ev);
}

int http1_next_event(http1_ctx_t *ctx, protocol_event_t *event)
{
    if (!ctx || !event || ctx->queue_count == 0) return 0;
    *event = ctx->event_queue[ctx->queue_head];
    ctx->queue_head = (ctx->queue_head + 1) % ctx->queue_size;
    ctx->queue_count--;
    return 1;
}

static int is_valid_header_name(const char *name)
{
    if (!name || *name == '\0') return 0;
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_') return 0;
    }
    return 1;
}

static int has_control_chars(const char *str)
{
    for (const char *p = str; *p; p++) {
        if (iscntrl(*p)) return 1;
    }
    return 0;
}

static int is_valid_method(const char *method)
{
    const char *valid[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH", NULL};
    for (int i = 0; valid[i]; i++) {
        if (strcmp(method, valid[i]) == 0) return 1;
    }
    return 0;
}

static void add_header(http1_ctx_t *ctx, const char *name, const char *value)
{
    if (ctx->header_count >= MAX_HEADERS) return;
    if (!is_valid_header_name(name)) {
        protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
        enqueue_event(ctx, &ev);
        return;
    }

    /* Reject control characters in header values */
    if (has_control_chars(value)) {
        protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
        enqueue_event(ctx, &ev);
        return;
    }

    /* Content-Length handling */
    if (strcasecmp(name, "content-length") == 0) {
        long new_len = atol(value);
        if (ctx->content_length_seen) {
            if (new_len != ctx->content_length) {
                protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
                enqueue_event(ctx, &ev);
            }
        } else {
            ctx->content_length = new_len;
            ctx->content_length_seen = 1;
        }
    }

    http1_hdr_t *h = &ctx->headers[ctx->header_count++];
    strncpy(h->name, name, MAX_HEADER_NAME - 1);

    while (*value == ' ' || *value == '\t') value++;
    strncpy(h->value, value, MAX_HEADER_VALUE - 1);

    if (strcasecmp(name, "upgrade") == 0 && strcasecmp(value, "websocket") == 0) {
        ctx->is_upgrade = 1;
        emit_event(ctx, HTTP1_EVENT_UPGRADE_DETECTED);
    }
}

int http1_set_header(http1_ctx_t *ctx, const char *name, const char *value)
{
    add_header(ctx, name, value);
    return 0;
}

int http1_get_header(const http1_ctx_t *ctx, const char *name, char *value, size_t max_len)
{
    for (int i = 0; i < ctx->header_count; i++) {
        if (strcasecmp(ctx->headers[i].name, name) == 0) {
            strncpy(value, ctx->headers[i].value, max_len);
            return 0;
        }
    }
    return -1;
}

int http1_is_websocket_upgrade(const http1_ctx_t *ctx)
{
    return ctx->is_upgrade;
}

void http1_initiate_websocket_upgrade(http1_ctx_t *ctx, const char *path, const char *host)
{
    if (ctx->role != HTTP1_ROLE_CLIENT) return;

    ctx->out_len = 0;
    char *p = (char *)ctx->out_buf;

    p += snprintf(p, MAX_OUT_BUF, "GET %s HTTP/1.1\r\n", path ? path : "/");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "Host: %s\r\n", host ? host : "localhost");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "Upgrade: websocket\r\n");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "Connection: Upgrade\r\n");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "Sec-WebSocket-Version: 13\r\n");
    p += snprintf(p, MAX_OUT_BUF - (p - (char*)ctx->out_buf), "\r\n");

    ctx->out_len = p - (char *)ctx->out_buf;
    ctx->state = HTTP1_STATE_HEADERS;
}

size_t http1_feed_input(http1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) return 0;

    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\r') continue;

        if (c == '\n') {
            ctx->line[ctx->line_pos] = '\0';

            if (ctx->line_pos > 0) {
                /* Server request line: any RFC method (GET/PUT/DELETE/…), not only GET. */
                if (ctx->role == HTTP1_ROLE_SERVER &&
                    sscanf(ctx->line, "%15s %255s %15s", ctx->method, ctx->path, ctx->version) == 3 &&
                    strncmp(ctx->version, "HTTP/1.", 7) == 0 &&
                    is_valid_method(ctx->method)) {
                    emit_event(ctx, HTTP1_EVENT_REQUEST_LINE);
                } else if (strstr(ctx->line, "HTTP/1.1 ") && ctx->role == HTTP1_ROLE_CLIENT) {
                    if (sscanf(ctx->line, "%15s %d %63[^\r\n]", ctx->version, &ctx->status_code, ctx->reason) != 3) {
                        protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
                        enqueue_event(ctx, &ev);
                    } else if (ctx->status_code < 100 || ctx->status_code > 599) {
                        protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
                        enqueue_event(ctx, &ev);
                    } else {
                        emit_event(ctx, HTTP1_EVENT_STATUS_LINE);
                    }
                } else if (strchr(ctx->line, ':')) {
                    char *colon = strchr(ctx->line, ':');
                    if (colon) {
                        *colon = '\0';
                        char *val = colon + 1;
                        while (*val == ' ' || *val == '\t') val++;
                        add_header(ctx, ctx->line, val);
                    }
                } else {
                    protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
                    enqueue_event(ctx, &ev);
                }
            } else {
                if (ctx->state == HTTP1_STATE_HEADERS) {
                    ctx->state = HTTP1_STATE_BODY;
                    emit_event(ctx, HTTP1_EVENT_HEADERS_COMPLETE);
                    ctx->body_bytes_received = 0;
                }
            }

            ctx->line_pos = 0;
            if (ctx->state == HTTP1_STATE_IDLE) ctx->state = HTTP1_STATE_HEADERS;
        } else {
            if (ctx->line_pos < LINE_BUF_SIZE - 1) {
                ctx->line[ctx->line_pos++] = c;
            } else {
                protocol_event_t ev = { .protocol = PROTOCOL_HTTP1, .type = PROTOCOL_EVENT_ERROR };
                enqueue_event(ctx, &ev);
                while (i < len && data[i] != '\n') i++;
                ctx->line_pos = 0;
            }
        }
    }

    /* Body collection */
    if (ctx->state == HTTP1_STATE_BODY && ctx->content_length > 0) {
        size_t to_take = len;
        if (ctx->body_bytes_received + (long)to_take > ctx->content_length) {
            to_take = ctx->content_length - ctx->body_bytes_received;
        }

        if (to_take > 0) {
            protocol_event_t ev;
            ev.protocol = PROTOCOL_HTTP1;
            ev.type = HTTP1_EVENT_BODY_CHUNK;
            ev.u.body.data = data;
            ev.u.body.len = to_take;
            enqueue_event(ctx, &ev);

            ctx->body_bytes_received += to_take;
        }

        if (ctx->body_bytes_received >= ctx->content_length) {
            ctx->state = HTTP1_STATE_COMPLETE;
        }
    }

    return len;
}

size_t http1_get_output(http1_ctx_t *ctx, uint8_t *buf, size_t max_len)
{
    if (!ctx || !buf || max_len == 0) return 0;
    size_t available = ctx->out_len - ctx->out_sent;
    size_t to_copy = available < max_len ? available : max_len;
    if (to_copy > 0) {
        memcpy(buf, ctx->out_buf + ctx->out_sent, to_copy);
        ctx->out_sent += to_copy;
    }
    return to_copy;
}

http1_state_t http1_current_state(const http1_ctx_t *ctx)
{
    return ctx ? ctx->state : HTTP1_STATE_ERROR;
}