#define _GNU_SOURCE
#include "http2.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* HTTP/2 frame types */
#define FRAME_DATA          0x0
#define FRAME_HEADERS       0x1
#define FRAME_PRIORITY      0x2
#define FRAME_RST_STREAM    0x3
#define FRAME_SETTINGS      0x4
#define FRAME_PUSH_PROMISE  0x5
#define FRAME_PING          0x6
#define FRAME_GOAWAY        0x7
#define FRAME_WINDOW_UPDATE 0x8
#define FRAME_CONTINUATION  0x9

/* Client connection preface (RFC 7540 Section 3.5) */
static const char CLIENT_PREFACE[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
#define CLIENT_PREFACE_LEN 24

/* HPACK static table (RFC 7541 Appendix A) - minimal useful subset */
static const char *static_headers[][2] = {
    {":authority",      ""},
    {":method",         "GET"},
    {":method",         "POST"},
    {":path",           "/"},
    {":path",           "/index.html"},
    {":scheme",         "http"},
    {":scheme",         "https"},
    {":status",         "200"},
    {":status",         "204"},
    {":status",         "206"},
    {":status",         "304"},
    {":status",         "400"},
    {":status",         "404"},
    {":status",         "500"},
    {"accept-charset",  ""},
    {"accept-encoding","gzip, deflate"},
    {"accept-language",""},
    {"accept-ranges",   ""},
    {"accept",          ""},
    {"access-control-allow-origin",""},
    {"age",             ""},
    {"allow",           ""},
    {"authorization",   ""},
    {"cache-control",   ""},
    {"content-disposition",""},
    {"content-encoding",""},
    {"content-language",""},
    {"content-length",  ""},
    {"content-location",""},
    {"content-range",   ""},
    {"content-type",    ""},
    {"cookie",          ""},
    {"date",            ""},
    {"etag",            ""},
    {"expect",          ""},
    {"expires",         ""},
    {"from",            ""},
    {"host",            ""},
    {"if-match",        ""},
    {"if-modified-since",""},
    {"if-none-match",   ""},
    {"if-range",        ""},
    {"if-unmodified-since",""},
    {"last-modified",   ""},
    {"link",            ""},
    {"location",        ""},
    {"max-forwards",    ""},
    {"proxy-authenticate",""},
    {"proxy-authorization",""},
    {"range",           ""},
    {"referer",         ""},
    {"refresh",         ""},
    {"retry-after",     ""},
    {"server",          ""},
    {"set-cookie",      ""},
    {"strict-transport-security",""},
    {"transfer-encoding",""},
    {"user-agent",      ""},
    {"vary",            ""},
    {"via",             ""},
    {"www-authenticate",""},
};

/* Internal HPACK dynamic table (very small for embedded use) */
#define DYN_TABLE_SIZE  4096
#define MAX_DYN_ENTRIES 64

typedef struct {
    char *name;
    char *value;
} hpack_entry_t;

typedef struct {
    hpack_entry_t entries[MAX_DYN_ENTRIES];
    int count;
    size_t size;
} hpack_dyn_table_t;

/* Frame parsing state */
typedef enum {
    PARSE_FRAME_HEADER = 0,
    PARSE_FRAME_PAYLOAD,
} frame_parse_state_t;

/* Per-stream state (simplified - single active stream for CRUD example) */
typedef enum {
    STREAM_IDLE = 0,
    STREAM_RESERVED,
    STREAM_OPEN,
    STREAM_HALF_CLOSED_REMOTE,
    STREAM_HALF_CLOSED_LOCAL,
    STREAM_CLOSED
} http2_stream_state_t;

typedef struct {
    uint32_t id;
    http2_stream_state_t state;
    uint32_t send_window;
    uint32_t recv_window;
} http2_stream_t;

#define MAX_STREAMS 32

/* Main context */
struct http2_ctx {
    http2_state_t state;
    uint32_t error_code;
    http2_role_t role;          /* ADR 004: client or server */
    int preface_sent;           /* for CLIENT role */

    /* Frame reader */
    frame_parse_state_t parse_state;
    uint8_t  frame_header[9];
    size_t   header_bytes_read;
    uint8_t *payload;
    size_t   payload_len;
    size_t   payload_read;
    uint32_t frame_type;
    uint8_t  frame_flags;
    uint32_t stream_id;

    /* HPACK */
    hpack_dyn_table_t hpack_table;

    /* Multi-stream support */
    http2_stream_t streams[MAX_STREAMS];
    int stream_count;

    /* Output buffer (library-generated frames) */
    uint8_t  out_buf[8192];
    size_t   out_len;
    size_t   out_sent;

    /* Received DATA buffer (exposed to application) */
    uint8_t *pending_data;
    size_t   pending_data_len;
    uint32_t pending_data_stream;
};

/* ------------------ HPACK minimal implementation ------------------ */

static void hpack_init(hpack_dyn_table_t *t)
{
    memset(t, 0, sizeof(*t));
}

static void hpack_free(hpack_dyn_table_t *t)
{
    for (int i = 0; i < t->count; i++) {
        free(t->entries[i].name);
        free(t->entries[i].value);
    }
    t->count = 0;
    t->size = 0;
}

static int hpack_add_entry(hpack_dyn_table_t *t, const char *name, const char *value)
{
    if (t->count >= MAX_DYN_ENTRIES) return -1;
    hpack_entry_t *e = &t->entries[t->count++];
    e->name  = strdup(name ? name : "");
    e->value = strdup(value ? value : "");
    t->size += strlen(name) + strlen(value) + 32;
    return 0;
}

/* Very small Huffman decoder stub (supports only literal strings for now) */
static int hpack_decode_string(const uint8_t *src, size_t src_len,
                               char *dst, size_t dst_max, size_t *consumed)
{
    /* For simplicity in this first expansion we only support
     * non-Huffman encoded strings (H bit = 0).
     * A full implementation would add Huffman decoding here.
     */
    if (src_len == 0) return -1;
    uint8_t first = src[0];
    int huff = (first & 0x80) != 0;
    size_t len = first & 0x7f;
    size_t offset = 1;

    if (len == 127) {
        /* 7+ bit length encoding - simplified, assume < 127 for example */
        return -1;
    }

    if (offset + len > src_len) return -1;
    if (len >= dst_max) return -1;

    if (huff) {
        /* TODO: implement real Huffman decoder */
        return -1;
    }

    memcpy(dst, src + offset, len);
    dst[len] = 0;
    *consumed = offset + len;
    return 0;
}

/* Decode indexed header or literal header (very minimal) */
static int hpack_decode_header(http2_ctx_t *ctx, const uint8_t *data, size_t len, size_t *consumed,
                               char *name_out, size_t name_max,
                               char *value_out, size_t value_max)
{
    if (len == 0) return -1;
    uint8_t b = data[0];
    *consumed = 0;

    if ((b & 0x80) == 0x80) {
        /* Indexed header field */
        uint32_t idx = b & 0x7f;
        if (idx == 0) return -1;
        if (idx <= 61) {
            /* static table */
            strncpy(name_out,  static_headers[idx-1][0], name_max);
            strncpy(value_out, static_headers[idx-1][1], value_max);
            *consumed = 1;
            return 0;
        } else {
            /* dynamic table */
            int d_idx = idx - 62;
            if (d_idx >= ctx->hpack_table.count) return -1;
            strncpy(name_out,  ctx->hpack_table.entries[d_idx].name,  name_max);
            strncpy(value_out, ctx->hpack_table.entries[d_idx].value, value_max);
            *consumed = 1;
            return 0;
        }
    } else if ((b & 0xc0) == 0x40) {
        /* Literal header field with incremental indexing */
        /* Simplified: assume name is indexed or literal */
        size_t off = 1;
        /* For this expansion we only handle indexed name + literal value */
        uint32_t name_idx = b & 0x3f;
        if (name_idx == 0) return -1; /* new name not supported in minimal version */

        if (name_idx <= 61) {
            strncpy(name_out, static_headers[name_idx-1][0], name_max);
        } else {
            return -1;
        }

        size_t vcons = 0;
        if (hpack_decode_string(data + off, len - off, value_out, value_max, &vcons) < 0)
            return -1;
        off += vcons;

        hpack_add_entry(&ctx->hpack_table, name_out, value_out);
        *consumed = off;
        return 0;
    } else {
        /* Other literal forms - treat as error for minimal implementation */
        return -1;
    }
}

/* Minimal HPACK encoder for response headers (literal encoding, no huffman/indexing) */
static int hpack_encode_header(uint8_t *buf, size_t max, const char *name, const char *value, int __attribute__((unused)) index)
{
    if (!buf || !name || !value) return 0;
    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    size_t needed = 2 + nlen + 1 + vlen;
    if (needed > max) return 0;

    size_t pos = 0;
    buf[pos++] = 0x00; /* literal header field with no indexing */
    buf[pos++] = (uint8_t)nlen;
    memcpy(buf + pos, name, nlen);
    pos += nlen;
    buf[pos++] = (uint8_t)vlen;
    memcpy(buf + pos, value, vlen);
    pos += vlen;
    return (int)pos;
}

/* ------------------ Stream management ------------------ */

static http2_stream_t *get_or_create_stream(http2_ctx_t *ctx, uint32_t stream_id)
{
    for (int i = 0; i < ctx->stream_count; i++) {
        if (ctx->streams[i].id == stream_id)
            return &ctx->streams[i];
    }
    if (ctx->stream_count >= MAX_STREAMS)
        return NULL;
    http2_stream_t *s = &ctx->streams[ctx->stream_count++];
    s->id = stream_id;
    s->state = STREAM_OPEN;
    s->send_window = 65535;
    s->recv_window = 65535;
    return s;
}

/* ------------------ SETTINGS handling ------------------ */

static void parse_settings(http2_ctx_t *ctx __attribute__((unused)), const uint8_t *payload, size_t len)
{
    /* SETTINGS frame payload is a sequence of 6-byte entries: (uint16_t id, uint32_t value) */
    for (size_t i = 0; i + 6 <= len; i += 6) {
        uint16_t id = (payload[i] << 8) | payload[i+1];
        uint32_t value __attribute__((unused)) = (payload[i+2] << 24) | (payload[i+3] << 16) |
                         (payload[i+4] << 8)  | payload[i+5];

        switch (id) {
        case 0x1: /* SETTINGS_HEADER_TABLE_SIZE */
            /* Future: resize HPACK dynamic table */
            break;
        case 0x2: /* SETTINGS_ENABLE_PUSH */
            /* We can ignore or store this */
            break;
        case 0x3: /* SETTINGS_MAX_CONCURRENT_STREAMS */
            /* Store if we want to enforce it */
            break;
        case 0x4: /* SETTINGS_INITIAL_WINDOW_SIZE */
            /* Update connection send window */
            break;
        case 0x5: /* SETTINGS_MAX_FRAME_SIZE */
            /* Enforce max frame size */
            break;
        case 0x6: /* SETTINGS_MAX_HEADER_LIST_SIZE */
            break;
        default:
            break;
        }
    }
}


static void parse_window_update(http2_ctx_t *ctx, const uint8_t *payload, size_t len, uint32_t stream_id) __attribute__((unused));

static void parse_window_update(http2_ctx_t *ctx, const uint8_t *payload, size_t len, uint32_t stream_id)
{
    if (len < 4) return;
    uint32_t increment = ((payload[0] & 0x7f) << 24) | (payload[1] << 16) |
                         (payload[2] << 8) | payload[3];
    if (stream_id != 0) {
        http2_stream_t *s = get_or_create_stream(ctx, stream_id);
        if (s) {
            s->recv_window += increment;
            if (s->recv_window > 0x7fffffff) s->recv_window = 0x7fffffff;
        }
    }
}

/* ------------------ GOAWAY and RST_STREAM ------------------ */

static void handle_goaway(http2_ctx_t *ctx, const uint8_t *payload, size_t len) __attribute__((unused));

static void handle_goaway(http2_ctx_t *ctx, const uint8_t *payload, size_t len)
{
    if (len < 8) return;
    uint32_t last_stream = ((payload[0] & 0x7f) << 24) | (payload[1] << 16) |
                           (payload[2] << 8) | payload[3];
    uint32_t error_code = (payload[4] << 24) | (payload[5] << 16) |
                          (payload[6] << 8) | payload[7];

    ctx->error_code = error_code;
    /* Mark all streams > last_stream as closed */
    for (int i = 0; i < ctx->stream_count; i++) {
        if (ctx->streams[i].id > last_stream) {
            ctx->streams[i].state = STREAM_CLOSED;
        }
    }
    ctx->state = HTTP2_STATE_CLOSED;
}

static void handle_rst_stream(http2_ctx_t *ctx, const uint8_t *payload, size_t len, uint32_t stream_id) __attribute__((unused));

static void handle_rst_stream(http2_ctx_t *ctx, const uint8_t *payload, size_t len, uint32_t stream_id)
{
    if (len < 4 || stream_id == 0) return;
    uint32_t error_code = (payload[0] << 24) | (payload[1] << 16) |
                          (payload[2] << 8) | payload[3];

    http2_stream_t *s = get_or_create_stream(ctx, stream_id);
    if (s) {
        s->state = STREAM_CLOSED;
    }
    if (ctx->error_code == 0) ctx->error_code = error_code;
}

/* ------------------ Frame handling ------------------ */

static void send_settings_ack(http2_ctx_t *ctx)
{
    if (ctx->out_len + 9 > sizeof(ctx->out_buf)) return;
    uint8_t *p = ctx->out_buf + ctx->out_len;
    p[0] = 0; p[1] = 0; p[2] = 0; /* length */
    p[3] = FRAME_SETTINGS;
    p[4] = 0x1; /* ACK */
    p[5] = p[6] = p[7] = p[8] = 0;
    ctx->out_len += 9;
}

static void send_headers_response(http2_ctx_t *ctx, uint32_t stream_id, const char *status __attribute__((unused)), const char *body) __attribute__((unused));

static void send_headers_response(http2_ctx_t *ctx, uint32_t stream_id, const char *status __attribute__((unused)), const char *body)
{
    if (ctx->role != HTTP2_ROLE_SERVER) return; /* only server generates responses in this minimal impl */

    http2_stream_t *s = get_or_create_stream(ctx, stream_id);
    if (!s) return;

    uint8_t *p = ctx->out_buf + ctx->out_len;
    size_t remaining = sizeof(ctx->out_buf) - ctx->out_len;
    if (remaining < 64) return;

    size_t body_len = strlen(body);
    char clen[32];
    snprintf(clen, sizeof(clen), "%zu", body_len);

    uint8_t headers[256];
    int hlen = 0;
    hlen += hpack_encode_header(headers + hlen, sizeof(headers) - hlen, ":status", "200", 0);
    hlen += hpack_encode_header(headers + hlen, sizeof(headers) - hlen, "content-length", clen, 0);

    p[0] = (hlen >> 16) & 0xff; p[1] = (hlen >> 8) & 0xff; p[2] = hlen & 0xff;
    p[3] = FRAME_HEADERS; p[4] = 0x4;
    p[5] = (stream_id >> 24) & 0x7f; p[6] = (stream_id >> 16) & 0xff;
    p[7] = (stream_id >> 8) & 0xff; p[8] = stream_id & 0xff;
    memcpy(p + 9, headers, hlen);
    ctx->out_len += 9 + hlen;

    if (remaining < 9 + body_len) return;
    p = ctx->out_buf + ctx->out_len;
    p[0] = (body_len >> 16) & 0xff; p[1] = (body_len >> 8) & 0xff; p[2] = body_len & 0xff;
    p[3] = FRAME_DATA; p[4] = 0x1; /* END_STREAM */
    p[5] = (stream_id >> 24) & 0x7f; p[6] = (stream_id >> 16) & 0xff;
    p[7] = (stream_id >> 8) & 0xff; p[8] = stream_id & 0xff;
    memcpy(p + 9, body, body_len);
    ctx->out_len += 9 + body_len;

    s->state = STREAM_HALF_CLOSED_LOCAL;
}

/* Process a fully received frame */
static void process_frame(http2_ctx_t *ctx)
{
    switch (ctx->frame_type) {
    case FRAME_SETTINGS:
        if ((ctx->frame_flags & 0x1) == 0) {
            parse_settings(ctx, ctx->payload, ctx->payload_len);
            if (ctx->role == HTTP2_ROLE_SERVER) {
                send_settings_ack(ctx);
            }
        }
        ctx->state = HTTP2_STATE_OPEN;
        break;

    case FRAME_HEADERS: {
        http2_stream_t *s = get_or_create_stream(ctx, ctx->stream_id);
        if (s) {
            s->state = STREAM_OPEN;
            char name[256], value[256];
            size_t off = 0;
            while (off < ctx->payload_len) {
                size_t cons = 0;
                if (hpack_decode_header(ctx, ctx->payload + off, ctx->payload_len - off,
                                        &cons, name, sizeof(name), value, sizeof(value)) == 0) {
                    off += cons;
                } else {
                    break;
                }
            }
        }
        break;
    }

    case FRAME_DATA:
        free(ctx->pending_data);
        ctx->pending_data = malloc(ctx->payload_len);
        if (ctx->pending_data) {
            memcpy(ctx->pending_data, ctx->payload, ctx->payload_len);
            ctx->pending_data_len = ctx->payload_len;
            ctx->pending_data_stream = ctx->stream_id;
        }
        break;

    case FRAME_PING:
        if (ctx->out_len + 17 <= sizeof(ctx->out_buf)) {
            uint8_t *p = ctx->out_buf + ctx->out_len;
            memcpy(p, ctx->payload - 9, 9);
            p[4] |= 0x1;
            memcpy(p + 9, ctx->payload, 8);
            ctx->out_len += 17;
        }
        break;

    case FRAME_GOAWAY:
        handle_goaway(ctx, ctx->payload, ctx->payload_len);
        break;

    case FRAME_RST_STREAM:
        handle_rst_stream(ctx, ctx->payload, ctx->payload_len, ctx->stream_id);
        break;

    case FRAME_WINDOW_UPDATE:
        parse_window_update(ctx, ctx->payload, ctx->payload_len, ctx->stream_id);
        break;

    default:
        break;
    }
}

/* ------------------ Public API ------------------ */

http2_ctx_t *http2_create(void)
{
    return http2_create_with_role(HTTP2_ROLE_SERVER);
}

http2_ctx_t *http2_create_with_role(http2_role_t role)
{
    http2_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->role = role;
        http2_reset(ctx);
    }
    return ctx;
}

void http2_destroy(http2_ctx_t *ctx)
{
    if (!ctx) return;
    hpack_free(&ctx->hpack_table);
    free(ctx->payload);
    free(ctx);
}

void http2_reset(http2_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->state = HTTP2_STATE_IDLE;
    ctx->error_code = 0;
    ctx->parse_state = PARSE_FRAME_HEADER;
    ctx->header_bytes_read = 0;
    ctx->payload_len = 0;
    ctx->payload_read = 0;
    free(ctx->payload);
    ctx->payload = NULL;
    ctx->out_len = 0;
    ctx->out_sent = 0;
    ctx->preface_sent = 0;
    hpack_free(&ctx->hpack_table);
    hpack_init(&ctx->hpack_table);
    ctx->stream_count = 0;
    for (int i = 0; i < MAX_STREAMS; i++) {
        ctx->streams[i].id = 0;
        ctx->streams[i].state = STREAM_IDLE;
        ctx->streams[i].send_window = 65535;
        ctx->streams[i].recv_window = 65535;
    }
    if (ctx->role == HTTP2_ROLE_CLIENT) {
        ctx->state = HTTP2_STATE_PREFACE;
    }
}

size_t http2_feed_input(http2_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) return 0;

    size_t total_consumed = 0;

    while (total_consumed < len) {
        if (ctx->state == HTTP2_STATE_IDLE && ctx->role == HTTP2_ROLE_SERVER) {
            if (len - total_consumed >= CLIENT_PREFACE_LEN) {
                ctx->state = HTTP2_STATE_PREFACE;
                total_consumed += CLIENT_PREFACE_LEN;
                continue;
            }
            return total_consumed;
        }

        if (ctx->parse_state == PARSE_FRAME_HEADER) {
            size_t need = 9 - ctx->header_bytes_read;
            size_t avail = len - total_consumed;
            size_t take = need < avail ? need : avail;
            memcpy(ctx->frame_header + ctx->header_bytes_read,
                   data + total_consumed, take);
            ctx->header_bytes_read += take;
            total_consumed += take;

            if (ctx->header_bytes_read == 9) {
                ctx->payload_len = (ctx->frame_header[0] << 16) |
                                   (ctx->frame_header[1] << 8) |
                                    ctx->frame_header[2];
                ctx->frame_type  = ctx->frame_header[3];
                ctx->frame_flags = ctx->frame_header[4];
                ctx->stream_id   = ((ctx->frame_header[5] & 0x7f) << 24) |
                                    (ctx->frame_header[6] << 16) |
                                    (ctx->frame_header[7] << 8) |
                                     ctx->frame_header[8];

                free(ctx->payload);
                ctx->payload = NULL;
                if (ctx->payload_len > 0) {
                    ctx->payload = malloc(ctx->payload_len);
                    if (!ctx->payload) {
                        ctx->state = HTTP2_STATE_ERROR;
                        ctx->error_code = 0x2;
                        return total_consumed;
                    }
                }
                ctx->payload_read = 0;
                ctx->parse_state = PARSE_FRAME_PAYLOAD;
            }
        }

        if (ctx->parse_state == PARSE_FRAME_PAYLOAD) {
            if (ctx->payload_len > 0) {
                size_t need = ctx->payload_len - ctx->payload_read;
                size_t avail = len - total_consumed;
                size_t take = need < avail ? need : avail;
                memcpy(ctx->payload + ctx->payload_read, data + total_consumed, take);
                ctx->payload_read += take;
                total_consumed += take;
            }

            if (ctx->payload_read == ctx->payload_len) {
                process_frame(ctx);
                ctx->parse_state = PARSE_FRAME_HEADER;
                ctx->header_bytes_read = 0;
            }
        }
    }
    return total_consumed;
}

http2_state_t http2_current_state(const http2_ctx_t *ctx)
{
    return ctx ? ctx->state : HTTP2_STATE_ERROR;
}

uint32_t http2_error_code(const http2_ctx_t *ctx)
{
    return ctx ? ctx->error_code : 0;
}

size_t http2_get_output(http2_ctx_t *ctx, uint8_t *buf, size_t max_len)
{
    if (!ctx || !buf || max_len == 0) return 0;

    if (ctx->out_sent >= ctx->out_len) {
        if (ctx->role == HTTP2_ROLE_CLIENT && !ctx->preface_sent) {
            size_t to_send = (max_len < CLIENT_PREFACE_LEN) ? max_len : CLIENT_PREFACE_LEN;
            memcpy(buf, CLIENT_PREFACE, to_send);
            ctx->preface_sent = 1;
            return to_send;
        }

        if (ctx->role == HTTP2_ROLE_SERVER &&
            ctx->state == HTTP2_STATE_OPEN && ctx->stream_count > 0) {
            const char *body = "Hello from real HTTP/2 state machine!";
            for (int i = 0; i < ctx->stream_count; i++) {
                if (ctx->streams[i].state == STREAM_OPEN) {
                    send_headers_response(ctx, ctx->streams[i].id, "200", body);
                    ctx->streams[i].state = STREAM_HALF_CLOSED_LOCAL;
                    break;
                }
            }
        }
    }

    size_t available = ctx->out_len - ctx->out_sent;
    size_t to_copy = available < max_len ? available : max_len;
    if (to_copy > 0) {
        memcpy(buf, ctx->out_buf + ctx->out_sent, to_copy);
        ctx->out_sent += to_copy;
    }
    return to_copy;
}

size_t http2_get_data(http2_ctx_t *ctx, uint8_t *buf, size_t max_len, uint32_t *stream_id)
{
    if (!ctx || !ctx->pending_data || !buf || max_len == 0) return 0;
    size_t to_copy = ctx->pending_data_len < max_len ? ctx->pending_data_len : max_len;
    memcpy(buf, ctx->pending_data, to_copy);
    if (stream_id) *stream_id = ctx->pending_data_stream;
    free(ctx->pending_data);
    ctx->pending_data = NULL;
    ctx->pending_data_len = 0;
    return to_copy;
}
