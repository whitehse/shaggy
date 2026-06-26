# ADR 005: HTTP/1.1 and WebSocket as Sibling Libraries (Exception to Core HTTP/2 Structure)

**Date**: 2026-06-24  
**Status**: Accepted (amended 2026-06-26)  
**Deciders**: Project maintainers

## Context
The core library (`src/http2.c`, `include/http2.h`) is a strict, pure HTTP/2 state machine. Requests were made to add cookie-based session support and WebSocket support directly into the HTTP/2 core. This was rejected because:

- Cookies are application-level header handling (already supported via HPACK).
- WebSocket uses a completely different framing protocol after an HTTP/1.1 Upgrade handshake and cannot coexist inside an HTTP/2 frame parser without violating the library's fundamental identity.

However, there is a legitimate need for WebSocket support in the overall project (especially for modern applications that start with an HTTP upgrade).

## Decision
We will implement HTTP/1.1 and WebSocket support as **separate sibling libraries** rather than modifying the HTTP/2 core:

- `include/http1.h` + `src/http1.c` — Minimal HTTP/1.1 request/response parser and serializer (buffer-driven, no syscalls).
- `include/websocket.h` + `src/websocket.c` — WebSocket framing state machine (client + server roles).

These new modules follow the same design principles as the HTTP/2 core:
- Pure state machines.
- Only byte buffers in/out.
- Explicit `*_ROLE_CLIENT` / `*_ROLE_SERVER` support (dialectic principle from ADR 004).
- No callbacks, no system calls.

The HTTP/2 core remains the primary focus but is no longer frozen. All three protocol modules should present a **consistent interface style** (config struct + event queue + `next_event` pattern).

## Rationale
- Preserves the purity and security properties of each protocol implementation.
- Allows WebSocket to be added without polluting the HTTP/2 state machine.
- Maintains the ability to test client ↔ server interactions using only in-memory buffer exchanges.
- Follows the spirit of "one protocol, one focused state machine".

## Consequences
- New source files and headers were added.
- CMake builds the additional static libraries (`http1`, `websocket`).
- New tests exercise HTTP/1.1 Upgrade → WebSocket framing.
- All three modules should converge on a similar public API shape:
  - `*_config_t` struct (e.g. `event_queue_size`)
  - `*_create_with_config(role, config)`
  - `*_next_event(ctx, &event)` returning a `protocol_event_t`
  - `*_feed_input` + `*_get_output`

## Verification
- All new code must compile with the same strict warning flags.
- Dialectic tests (client ↔ server) must pass using only buffer hand-off.
- The HTTP/2 core **may** receive updates when needed to improve the protocol state machine, add missing features, or align interfaces. The important principle is **module separation**: HTTP/1.1 lives in its own files, WebSocket lives in its own files, and HTTP/2 lives in its own files. They should present similar high-level interfaces (config structs + event-driven `next_event` style) but remain independent implementations.

This ADR records the multi-protocol structure while protecting the "one protocol, one focused state machine" philosophy. The original restriction against touching `http2.c`/`http2.h` is rescinded; the goal was module separation, not freezing the HTTP/2 implementation.