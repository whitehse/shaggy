# ARCHITECTURE.md — libhttp2

## Core Principle
This is a **pure state machine** implementation of HTTP/2 (RFC 7540/7541), with sibling libraries for HTTP/1.1 and WebSocket. No function pointers for callbacks, no direct system calls, no hidden allocations beyond what's explicitly requested via config. The design is directly influenced by libassh's explicit state-machine approach and follows the "plumbing" philosophy (ADR 006): the libraries are thin PDU parsers/serializers that emit structured events and unencapsulated data; the calling application owns all policy, decisions, and side effects.

## Module Boundaries
- `include/http2.h` + `src/http2.c` — Core HTTP/2 state machine (untouched per ADR 005). Opaque context, buffer-in/buffer-out.
- `include/http1.h` + `src/http1.c` — Minimal HTTP/1.1 parser/serializer, primarily for Upgrade handshakes to WebSocket.
- `include/websocket.h` + `src/websocket.c` — WebSocket framing state machine (client + server roles, fragmentation support).
- `include/protocol_events.h` — Shared event structures (`protocol_event_t`) used by all modules for explicit, queueable output.
- All modules follow identical patterns: `*_create` / `*_create_with_config`, `*_feed_input`, `*_next_event`, `*_get_output`, `*_destroy`, `*_reset`.

## Configuration and Initialization (ADR 006 / User Preference)
- Libraries use config structs at creation time for configurability (e.g., `websocket_config_t { int event_queue_size; }`).
- Default constructors (`websocket_create`) use sensible defaults (queue size 8).
- `*_create_with_config` returns NULL on invalid config (queue_size <= 0) or allocation failure — caller must check.
- This enables event queue sizing, future allocator hooks, etc., without hard-coded defaults inside the library.

## Event-Driven Plumbing Model (ADR 006)
- Primary output mechanism: `int *_next_event(ctx, protocol_event_t *event)` — returns 1 if an event was dequeued and filled, 0 otherwise.
- Events (`WS_EVENT_TEXT`, `WS_EVENT_BINARY`, `WS_EVENT_PING`, etc.; similar for HTTP1) deliver unencapsulated data (`ws_message_t { const uint8_t *data; size_t len; }`).
- No auto-ACKs, auto-PONGs, or response generation inside the library. Parsed data is returned raw for the caller to act upon.
- Send helpers (`websocket_send_text`, `websocket_send_ping`, ...) queue frames for output; they return 0 on success, -1 on error (NULL ctx, etc.).
- `*_get_output` returns bytes copied (0 if none).
- `*_feed_input` returns bytes consumed; internally enqueues protocol events.

## Invariants
- The library never blocks, never calls `read`/`write`/`socket` (malloc/realloc used internally for fragmentation/unmasking only where unavoidable for correctness; no I/O syscalls).
- All I/O and event loop integration is the responsibility of the caller. Caller feeds contiguous byte buffers into `*_feed_input` and drains via `*_get_output`.
- State transitions are deterministic. Error handling via events (WS_EVENT_ERROR) or state queries.
- Dialectic testing: all tests use paired client/server contexts exchanging buffers in-memory (see ADR 004).

## Deliberate Absences (by design)
- No TLS/crypto, no multiplexing beyond single context (caller orchestrates), no dynamic policy.
- No callbacks — progress is always pull-driven via next_event / state queries.

## Entry Points (Common Across Modules)
- Creation: `*_create(role)`, `*_create_with_config(role, config)` → ctx or NULL
- Teardown: `*_destroy(ctx)`, `*_reset(ctx)`
- I/O: `size_t *_feed_input(ctx, data, len)`, `size_t *_get_output(ctx, buf, max_len)`
- Events: `int *_next_event(ctx, protocol_event_t *event)` → 1/0
- State: `*_current_state(ctx)`
- Protocol-specific sends and helpers

## How the State Machine Works
Each module maintains explicit states. Input bytes are parsed, events enqueued, output frames serialized on send calls. Caller drives: feed → next_event loop until no more events, drain output, repeat. Supports edge-triggered and completion-based loops (ADR 002).

## HTTP/2 Channel (Stream) Edge Cases
The core HTTP/2 implementation (`src/http2.c`) provides a simplified stream multiplexing model (up to `MAX_STREAMS=32`, per-stream send/recv windows, basic GOAWAY/RST_STREAM handling). The following edge cases exist in the current skeleton and should be considered when using or extending the channel handling:

- **Stream ID allocation & parity**: Client streams are odd, server streams even; stream 0 is connection-scoped. No validation in `get_or_create_stream`.
- **Stream state machine**: IDLE / OPEN / HALF_CLOSED_* / CLOSED states defined but transitions on END_STREAM, RST_STREAM, and flag combinations are incomplete.
- **Flow control**: Initial windows = 65535. WINDOW_UPDATE updates recv_window with overflow clamp, but send_window decrement on DATA and blocking behavior are not fully enforced in visible paths.
- **CONTINUATION frames**: Large HEADERS that exceed a single frame require CONTINUATION reassembly — not implemented.
- **SETTINGS & HPACK**: Most SETTINGS parameters (HEADER_TABLE_SIZE, MAX_CONCURRENT_STREAMS, INITIAL_WINDOW_SIZE, MAX_FRAME_SIZE) are parsed as no-ops. Dynamic table resizing is future work.
- **GOAWAY & graceful shutdown**: Marks streams > last_stream CLOSED; does not fully track in-flight streams or pending responses.
- **Priority / dependency**: FRAME_PRIORITY defined but no tree or re-prioritization logic.
- **Connection preface & initial SETTINGS exchange**: Client preface constant exists; server-side validation and SETTINGS ACK generation are minimal.
- **Error surface**: Unknown frame types, invalid flags, padding, and precise PROTOCOL_ERROR generation are limited.
- **Resource limits**: Hard 32-stream cap with no SETTINGS_MAX_CONCURRENT_STREAMS enforcement.

These limitations are documented so that callers and future extensions (always in sibling modules) understand the current scope. Full RFC 7540 compliance would require additional state machine work around these areas while preserving the pure buffer-driven contract.

## Future Growth
- Additional protocols as new sibling modules only (new ADR required).
- Event queue backpressure handling, more config options.
- When the HTTP/2 core is extended, all stream/channel edge cases above must be addressed together with documentation and manpage updates (ADR 007).

## Documentation and Manpages
See `docs/` and `man/man3/` (installed as section 3 manpages) for C API details, return codes, and calling conventions. All public functions document NULL checks, return values (0 success / -1 error / bytes / 1 event), and event semantics.

This architecture guarantees pure byte-buffer testability, maximum reusability as plumbing, and strict adherence to no-syscall / no-callback rules.