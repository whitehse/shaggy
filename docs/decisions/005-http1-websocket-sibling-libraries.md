# ADR 005: HTTP/1.1 and WebSocket as Sibling Libraries (Exception to Core HTTP/2 Structure)

**Date**: 2026-06-24  
**Status**: Accepted  
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

The HTTP/2 core remains untouched. The new HTTP/1.1 layer is primarily intended to support the WebSocket Upgrade handshake (and basic request/response for testing).

## Rationale
- Preserves the purity and security properties of the HTTP/2 implementation.
- Allows WebSocket to be added without polluting the HTTP/2 state machine.
- Maintains the ability to test client ↔ server interactions using only in-memory buffer exchanges.
- Follows the spirit of "one protocol, one focused state machine".

## Consequences
- New source files and headers will be added.
- CMake will build the additional static libraries/targets (`http1`, `websocket`).
- New tests will be added under `tests/` exercising HTTP/1.1 Upgrade → WebSocket framing in both directions.
- Documentation (ARCHITECTURE.md, DOMAIN.md) will be updated to describe the multi-protocol structure.
- Future protocol additions should follow the same pattern (new focused state machine + ADR).

## Verification
- All new code must compile with the same strict warning flags.
- Dialectic tests (client ↔ server) must pass using only buffer hand-off.
- No changes are allowed to `src/http2.c` or `include/http2.h`.

This ADR formally records the exception to the "single core library" rule while protecting the original HTTP/2 design.