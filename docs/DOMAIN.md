# docs/DOMAIN.md — libhttp2 Domain Glossary

**Note to human reviewer**: Expanded to cover HTTP/2 core + HTTP/1.1 and WebSocket siblings per ADR 005, and the plumbing/event model per ADR 006.

## Core HTTP/2 Concepts
- **Frame**: Smallest protocol unit (HEADERS, DATA, SETTINGS, PING, etc.). 9-byte header + payload.
- **Stream**: Bidirectional sequence of frames sharing a stream identifier.
- **Connection preface**: Magic string that must be sent first by client.
- **HPACK**: Header compression algorithm (static + dynamic table).
- **Flow control**: Per-stream and per-connection WINDOW_UPDATE mechanism.
- **States** (connection & stream): IDLE, RESERVED, OPEN, HALF_CLOSED, CLOSED, ERROR.

## WebSocket Concepts (Sibling Module)
- **Frame**: FIN + opcode + masked payload (TEXT=0x1, BINARY=0x2, PING=0x9, PONG=0xA, CLOSE=0x8).
- **Roles**: Client (masks outgoing) vs Server.
- **Fragmentation**: Multi-frame messages via CONTINUATION.
- **Events emitted**: WS_EVENT_TEXT, WS_EVENT_BINARY, WS_EVENT_PING, WS_EVENT_PONG, WS_EVENT_CLOSE, WS_EVENT_ERROR.
- **Upgrade**: Initiated via HTTP/1.1 Upgrade header (handled in http1 sibling).

## HTTP/1.1 Concepts (Sibling Module)
- **Request/Response lines**, headers, body chunks.
- **Events**: HTTP1_EVENT_REQUEST_LINE, HTTP1_EVENT_STATUS_LINE, HTTP1_EVENT_HEADER, HTTP1_EVENT_HEADERS_COMPLETE, HTTP1_EVENT_BODY_CHUNK, HTTP1_EVENT_UPGRADE_DETECTED.
- **Upgrade detection**: `http1_is_websocket_upgrade` helper for WebSocket handshake.

## Shared Plumbing / Event Model (ADR 006)
- **protocol_event_t**: Unified structure with `protocol` type, `type`, and union of message/close/body data. Data pointers are valid only until next feed or destroy (unencapsulated, caller-owned interpretation).
- **next_event(ctx, &ev)**: Returns 1 on event (dequeued from configurable queue), 0 if none. Never generates side effects or responses.
- **Configurable queues**: event_queue_size passed at *_create_with_config time to handle backpressure in high-throughput scenarios.
- All modules return raw parsed data; caller decides ACKs, PONGs, responses, routing, etc.

## Workflows the Libraries Must Support
1. HTTP/2: Client preface + SETTINGS → server processes → emits events for headers/data.
2. HTTP/1.1 Upgrade → WebSocket framing switch (dialectic test in test_http1_websocket.c).
3. WebSocket: TEXT/BINARY exchange, PING/PONG, CLOSE handshake, fragmentation — all via events and explicit sends.
4. Error conditions → WS_EVENT_ERROR or HTTP1 error events with precise codes.
5. Dialectic client/server buffer exchange for all protocol pairs (ADR 004).

## Terminology Used in Code
- `*_ctx_t` — Opaque per-connection state machine context (one per protocol module).
- `*_config_t` — Initialization config (currently event_queue_size).
- `*_feed_input` / `*_get_output` / `*_next_event` — The core I/O and event surfaces.
- `protocol_event_t` — The structured event emitted to callers.

## Open Questions / Extensions
- Exact memory model (current: internal calloc for ctx + queue + fragments).
- Additional config (max frame size, etc.).
- Full HTTP/2 stream multiplexing context (future sibling or extension).

All documentation emphasizes C calling conventions: check returns for NULL/0/-1/bytes, events carry const data pointers, no ownership transfer unless documented. See manpages for precise signatures and error semantics.