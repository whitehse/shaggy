# ADR 010: C Interfaces and Implementations + Language Binding Friendly Design

**Date**: 2026-06-26  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
The library is a pure C state machine for HTTP/2 (and sibling protocols). As the project matures and the interface is being unified across `http1`, `http2`, and `websocket` (see ADR 009), it is important to establish long-term design principles for the public API.

Two influences are particularly relevant:

1. **C Interfaces and Implementations** (David R. Hanson, 1996) — A foundational text on writing reusable, well-engineered C libraries. It emphasizes opaque types, consistent naming, minimal interfaces, clear ownership, and separation of interface from implementation.

2. **Language Binding Friendliness** — Modern protocol libraries are frequently consumed via FFI from Rust, Go, Python, Zig, and other languages. Poor C API design makes binding generation difficult or unsafe.

## Decision
The library shall adopt the following principles:

### From *C Interfaces and Implementations*
- **Opaque types**: All contexts (`http2_ctx_t`, `http1_ctx_t`, `websocket_ctx_t`) remain completely opaque. Clients never access struct members directly.
- **Consistent naming**: All public symbols use a clear module prefix (`http2_`, `http1_`, `websocket_`).
- **Minimal interfaces**: Each module exposes only what is necessary. Implementation details stay in `*.c` files.
- **Clear ownership**: Functions that return newly allocated objects document who is responsible for calling the corresponding `*_destroy` function.
- **Error handling**: Prefer explicit return values or out-parameters over hidden global state.

### Language Binding Support
- Public headers shall avoid:
  - Complex macros that expand to code
  - Bitfields in public structures
  - Inline functions that expose implementation details
  - C++ reserved words or GNU extensions in public APIs
- All handles are passed as pointers (`T *`).
- Function signatures should be straightforward for tools like `bindgen`, `cffi`, and `ctypes`.
- Event structures (`protocol_event_t`) use simple unions and fixed-size arrays where possible.

## Rationale
- Hanson's book provides battle-tested patterns for writing reusable C that have stood the test of time.
- Designing for FFI from the start reduces long-term maintenance burden when bindings are inevitably requested.
- These principles reinforce the existing "plumbing" philosophy: thin, predictable, buffer-in/buffer-out interfaces.
- Opaque types + consistent naming make the three protocol modules feel like a cohesive family while remaining independent.

## Consequences
- Future changes to `include/http2.h`, `include/http1.h`, and `include/websocket.h` must be reviewed against these constraints.
- The `protocol_event_t` union and config structs are intentionally simple to aid binding generation.
- Documentation (especially header comments) must explicitly state ownership and error semantics.
- When adding new features, preference will be given to designs that remain easy to bind.

## Verification
- All public headers continue to use opaque types and consistent prefixes.
- New public functions follow the established naming and ownership patterns.
- The library remains easy to consume from at least one non-C language (future verification target).

This decision strengthens both the internal quality and the external usability of the library.