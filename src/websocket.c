#include "websocket.h"
#include "protocol_events.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WS_MAX_FRAME_SIZE 65536
#define WS_DEFAULT_EVENT_QUEUE_SIZE 8
#define WS_MAX_FRAGMENT_SIZE (1024 * 1024)

struct websocket_ctx {
    ws_role_t role;
    ws_state_t state;

    uint8_t out_buf[WS_MAX_FRAME_SIZE];
    size_t out_len;
    size_t out_sent;

    protocol_event_t *event_queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    int queue_count;

    uint8_t *fragment_buffer;
    size_t fragment_len;
    size_t fragment_capacity;
    ws_opcode_t fragment_opcode;
    int in_fragment;

    uint8_t mask_key[4];
    int mask_index;
};

static int ensure_fragment_capacity(websocket_ctx_t *ctx, size_t needed)
{
    if (ctx->fragment_capacity >= needed) return 1;
    size_t new_cap = ctx->fragment_capacity ? ctx->fragment_capacity * 2 : 4096;
    while (new_cap < needed) new_cap *= 2;
    if (new_cap > WS_MAX_FRAGMENT_SIZE) return 0;
    uint8_t *new_buf = realloc(ctx->fragment_buffer, new_cap);
    if (!new_buf) return 0;
    ctx->fragment_buffer = new_buf;
    ctx->fragment_capacity = new_cap;
    return 1;
}

static void enqueue_event(websocket_ctx_t *ctx, const protocol_event_t *ev)
{
    if (ctx->queue_count >= ctx->queue_size) return;
    ctx->event_queue[ctx->queue_tail] = *ev;
    ctx->queue_tail = (ctx->queue_tail + 1) % ctx->queue_size;
    ctx->queue_count++;
}

websocket_ctx_t *websocket_create(ws_role_t role)
{
    websocket_config_t default_config = { .event_queue_size = WS_DEFAULT_EVENT_QUEUE_SIZE };
    return websocket_create_with_config(role, &default_config);
}

websocket_ctx_t *websocket_create_with_config(ws_role_t role, const websocket_config_t *config)
{
    if (!config || config->event_queue_size <= 0) return NULL;
    websocket_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->event_queue = calloc(config->event_queue_size, sizeof(protocol_event_t));
    if (!ctx->event_queue) { free(ctx); return NULL; }
    ctx->role = role;
    ctx->queue_size = config->event_queue_size;
    websocket_reset(ctx);
    return ctx;
}

void websocket_destroy(websocket_ctx_t *ctx)
{
    if (ctx) {
        free(ctx->event_queue);
        free(ctx->fragment_buffer);
        free(ctx);
    }
}

void websocket_reset(websocket_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->state = WS_STATE_IDLE;
    ctx->out_len = 0;
    ctx->out_sent = 0;
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_count = 0;
    ctx->in_fragment = 0;
    ctx->fragment_len = 0;
    ctx->mask_index = 0;
    ctx->mask_key[0] = 0x12; ctx->mask_key[1] = 0x34;
    ctx->mask_key[2] = 0x56; ctx->mask_key[3] = 0x78;
}

static void emit_frame(websocket_ctx_t *ctx, ws_opcode_t opcode, const uint8_t *payload, size_t len, int fin)
{
    if (ctx->out_len + 14 + len > sizeof(ctx->out_buf)) return;
    uint8_t *p = ctx->out_buf + ctx->out_len;
    size_t start = ctx->out_len;
    p[0] = (fin ? 0x80 : 0x00) | (opcode & 0x0F);
    int mask_bit = (ctx->role == WS_ROLE_CLIENT) ? 0x80 : 0x00;
    if (len <= 125) {
        p[1] = (uint8_t)len | mask_bit; p += 2;
    } else if (len <= 65535) {
        p[1] = 126 | mask_bit;
        p[2] = (len >> 8) & 0xFF; p[3] = len & 0xFF; p += 4;
    } else return;
    if (ctx->role == WS_ROLE_CLIENT) {
        memcpy(p, ctx->mask_key, 4); p += 4;
        for (size_t i = 0; i < len; i++) p[i] = payload[i] ^ ctx->mask_key[i % 4];
    } else {
        memcpy(p, payload, len);
    }
    ctx->out_len += (p - (ctx->out_buf + start)) + len;
}

int websocket_send_text(websocket_ctx_t *ctx, const char *text)
{
    if (!ctx || !text) return -1;
    emit_frame(ctx, WS_OP_TEXT, (const uint8_t*)text, strlen(text), 1);
    return 0;
}

int websocket_send_binary(websocket_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data) return -1;
    emit_frame(ctx, WS_OP_BINARY, data, len, 1);
    return 0;
}

int websocket_send_ping(websocket_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx) return -1;
    emit_frame(ctx, WS_OP_PING, data, len ? len : 0, 1);
    return 0;
}

int websocket_send_pong(websocket_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx) return -1;
    emit_frame(ctx, WS_OP_PONG, data, len ? len : 0, 1);
    return 0;
}

int websocket_send_close(websocket_ctx_t *ctx, uint16_t code, const uint8_t *reason, size_t reason_len)
{
    if (!ctx) return -1;
    uint8_t payload[128] = { (code >> 8) & 0xFF, code & 0xFF };
    size_t total = 2;
    if (reason && reason_len > 0 && reason_len < 126) {
        memcpy(payload + 2, reason, reason_len);
        total += reason_len;
    }
    emit_frame(ctx, WS_OP_CLOSE, payload, total, 1);
    ctx->state = WS_STATE_CLOSING;
    return 0;
}

int websocket_send_frame(websocket_ctx_t *ctx, ws_opcode_t opcode, int fin, const uint8_t *payload, size_t len)
{
    if (!ctx) return -1;
    emit_frame(ctx, opcode, payload, len, fin);
    return 0;
}

static size_t process_one_frame(websocket_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (len < 2 || ctx->queue_count >= ctx->queue_size) return 0;

    uint8_t b0 = data[0];
    uint8_t b1 = data[1];
    int fin = (b0 & 0x80) != 0;
    ws_opcode_t opcode = (ws_opcode_t)(b0 & 0x0F);
    int rsv = (b0 & 0x70) != 0;
    int masked = (b1 & 0x80) != 0;
    uint64_t plen = b1 & 0x7F;

    if (rsv || (opcode >= 0x3 && opcode <= 0x7)) {
        protocol_event_t ev = { .protocol = PROTOCOL_WEBSOCKET, .type = WS_EVENT_ERROR };
        enqueue_event(ctx, &ev);
        return len;
    }

    size_t header_len = 2;
    if (plen == 126) header_len += 2;
    if (masked) header_len += 4;
    if (len < header_len) return 0;

    const uint8_t *mask = masked ? data + header_len - 4 : NULL;
    size_t payload_start = header_len;
    if (plen == 126) plen = (data[2] << 8) | data[3];
    if (len < payload_start + plen) return 0;

    uint8_t *unmasked = malloc(plen ? plen : 1);
    if (!unmasked) return 0;
    for (size_t i = 0; i < plen; i++) unmasked[i] = data[payload_start + i] ^ (mask ? mask[i % 4] : 0);

    protocol_event_t ev;
    ev.protocol = PROTOCOL_WEBSOCKET;
    ev.type = PROTOCOL_EVENT_NONE;

    if (opcode == WS_OP_TEXT || opcode == WS_OP_BINARY) {
        if (ctx->in_fragment && opcode != WS_OP_CONTINUATION) {
            ctx->in_fragment = 0;
            ctx->fragment_len = 0;
        }
        if (fin) {
            ev.type = (opcode == WS_OP_TEXT) ? WS_EVENT_TEXT : WS_EVENT_BINARY;
            ev.u.ws_message.data = unmasked;
            ev.u.ws_message.len = plen;
            enqueue_event(ctx, &ev);
        } else {
            if (!ensure_fragment_capacity(ctx, plen)) { free(unmasked); return header_len + plen; }
            memcpy(ctx->fragment_buffer, unmasked, plen);
            ctx->fragment_len = plen;
            ctx->fragment_opcode = opcode;
            ctx->in_fragment = 1;
            free(unmasked);
        }
    } else if (opcode == WS_OP_CONTINUATION) {
        if (!ctx->in_fragment) { free(unmasked); return header_len + plen; }
        if (!ensure_fragment_capacity(ctx, ctx->fragment_len + plen)) { free(unmasked); return header_len + plen; }
        memcpy(ctx->fragment_buffer + ctx->fragment_len, unmasked, plen);
        ctx->fragment_len += plen;
        free(unmasked);
        if (fin) {
            ev.type = (ctx->fragment_opcode == WS_OP_TEXT) ? WS_EVENT_TEXT : WS_EVENT_BINARY;
            ev.u.ws_message.data = ctx->fragment_buffer;
            ev.u.ws_message.len = ctx->fragment_len;
            enqueue_event(ctx, &ev);
            ctx->in_fragment = 0;
            ctx->fragment_len = 0;
        }
    } else if (opcode == WS_OP_PING) {
        ev.type = WS_EVENT_PING;
        ev.u.ws_message.data = unmasked;
        ev.u.ws_message.len = plen;
        enqueue_event(ctx, &ev);
    } else if (opcode == WS_OP_PONG) {
        ev.type = WS_EVENT_PONG;
        ev.u.ws_message.data = unmasked;
        ev.u.ws_message.len = plen;
        enqueue_event(ctx, &ev);
    } else if (opcode == WS_OP_CLOSE) {
        ev.type = WS_EVENT_CLOSE;
        ev.u.ws_close.code = (plen >= 2) ? ((unmasked[0]<<8)|unmasked[1]) : 1000;
        ev.u.ws_close.reason = (plen > 2) ? unmasked+2 : NULL;
        ev.u.ws_close.reason_len = (plen > 2) ? plen-2 : 0;
        enqueue_event(ctx, &ev);
        if (ctx->state == WS_STATE_CLOSING) ctx->state = WS_STATE_CLOSED;
        else ctx->state = WS_STATE_CLOSING;
    } else {
        free(unmasked);
    }

    return header_len + plen;
}

size_t websocket_feed_input(websocket_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data) return 0;
    size_t total_consumed = 0;
    while (total_consumed < len && ctx->queue_count < ctx->queue_size) {
        size_t consumed = process_one_frame(ctx, data + total_consumed, len - total_consumed);
        if (consumed == 0) break;
        total_consumed += consumed;
    }
    return total_consumed;
}

int websocket_next_event(websocket_ctx_t *ctx, protocol_event_t *event)
{
    if (!ctx || !event || ctx->queue_count == 0) return 0;
    *event = ctx->event_queue[ctx->queue_head];
    ctx->queue_head = (ctx->queue_head + 1) % ctx->queue_size;
    ctx->queue_count--;
    return 1;
}

size_t websocket_get_output(websocket_ctx_t *ctx, uint8_t *buf, size_t max_len)
{
    if (!ctx || !buf) return 0;
    size_t available = ctx->out_len - ctx->out_sent;
    size_t to_copy = available < max_len ? available : max_len;
    if (to_copy > 0) {
        memcpy(buf, ctx->out_buf + ctx->out_sent, to_copy);
        ctx->out_sent += to_copy;
    }
    return to_copy;
}

ws_state_t websocket_current_state(const websocket_ctx_t *ctx)
{
    return ctx ? ctx->state : WS_STATE_ERROR;
}