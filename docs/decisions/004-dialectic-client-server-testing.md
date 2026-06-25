# ADR 004: Dialectic Client+Server Implementation for Protocol Code

**Date**: 2026-06-24  
**Status**: Accepted  
**Deciders**: Project maintainers (via agent directive)

## Context
HTTP/2 (and similar stateful protocols) are inherently two-sided. Implementing only the server side (or only client) risks incomplete protocol coverage, asymmetric bugs, and tests that cannot exercise full request/response cycles without external dependencies (real sockets, event loops, or third-party clients).

The existing scaffold had partial server-oriented logic (SETTINGS ACK, canned response generation) but no explicit client role, preface emission from client, or paired testing. Changes to the core state machine must remain pure (buffer-driven, no syscalls).

## Decision
Any implementation or extension that touches network protocol logic **must** implement symmetric support for both sides of the connection (client and server roles) inside the same state machine / library, at minimum to enable dialectic (paired) testing.

- The library exposes a single `http2_ctx_t` that can be instantiated in either `HTTP2_ROLE_CLIENT` or `HTTP2_ROLE_SERVER` mode.
- Client mode: emits connection preface on first `http2_get_output`, drives request initiation, processes server responses.
- Server mode: consumes preface, emits ACKs and responses.
- All testing of new frame/HPACK/state logic uses paired in-memory client_ctx ↔ server_ctx exchanges (feed one's output buffer directly into the other's `http2_feed_input`). No sockets, no blocking, no external processes.
- This principle is now a first-class architectural requirement alongside "no syscalls" and "pure state machine."

## Rationale
- Forces complete, symmetric protocol coverage.
- Enables hermetic, reproducible tests that exercise both code paths without network stack.
- Catches client/server asymmetry bugs early (e.g., preface handling, stream ID allocation, flow control directions).
- Aligns with the "pure buffer" design: the test harness is just a simple loop shuttling byte arrays.
- Prevents future one-sided features that would be hard to test or verify.

## Consequences
- `http2_create` (or new `http2_create_with_role`) accepts a role parameter; internal state machine branches on role for preface, stream ID init (client starts at 1, server at 2), response vs request generation, etc.
- New tests in `tests/test_dialectic.c` (or extended `test_http2.c`) must demonstrate client↔server roundtrips.
- Documentation (DOMAIN.md, ARCHITECTURE.md, AGENTS.md) updated to reference the dialectic requirement.
- Future protocol extensions (e.g., new frame types) must be exercised from both roles in tests.
- No change to the "all I/O lives in caller" rule — role only affects internal generation of preface/headers.

## Verification
- `ctest` includes dialectic tests that pass with two contexts exchanging buffers.
- Build remains clean under strict warnings.
- No new syscalls or callbacks introduced.
- ADR referenced in future changes touching client/server paths.

This decision codifies the "dialectic development" mandate so that client and server code evolve together and are validated against each other.