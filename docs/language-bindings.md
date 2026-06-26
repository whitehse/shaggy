# Language Bindings for libhttp2

This document describes how to create language bindings for the libhttp2 protocol library, following the principles established in **ADR 010**.

## Design Goals

- **Opaque handles**: All contexts (`http2_ctx_t`, etc.) are opaque pointers. Bindings should never expose internal struct members.
- **Consistent naming**: All functions use a `http2_` (or `http1_`, `websocket_`) prefix.
- **Event-driven model**: Prefer `*_next_event(ctx, &event)` over legacy getters.
- **FFI friendly**: Public headers avoid complex macros, bitfields, and C++-specific constructs.
- **Clear ownership**: Every `*_create*` has a corresponding `*_destroy`.

## Recommended Binding Patterns

### 1. Opaque Handle Wrapping

```rust
// Rust example
pub struct Http2Context {
    inner: *mut http2_ctx_t,
}
```

```perl
# Perl example
package AnyEvent::HTTP2;
use FFI::Platypus;
# $ctx is an opaque pointer
```

### 2. Event Handling

Bindings should expose an event loop integration point. The recommended pattern is:

```c
int http2_next_event(http2_ctx_t *ctx, protocol_event_t *event);
```

Bindings should map `protocol_event_t` to native event objects.

### 3. Configuration

Use `*_create_with_config` rather than the simple `*_create` when the binding wants to expose tuning parameters (e.g., `event_queue_size`).

### 4. Error Handling

- Protocol errors are surfaced via `HTTP2_EVENT_ERROR` in the event stream.
- Allocation failures and invalid arguments should be mapped to language-native exceptions or error results.

## General Recommendations

- Use **FFI** (Foreign Function Interface) tools where possible (`FFI::Platypus` in Perl, `ctypes`/`cffi` in Python, `bindgen` in Rust).
- Avoid writing custom XS/C glue unless performance requirements demand it.
- Provide both a low-level binding (thin wrapper) and a higher-level ergonomic API.
- Document ownership semantics clearly in the binding documentation.

## Adding a New Binding

1. Create a new directory under `bindings/` (e.g., `bindings/perl/`).
2. Add a `README.md` or language-specific document in `docs/`.
3. Ensure the binding follows the opaque handle + event model.
4. Update this document with a link to the new binding.

See `docs/perl-binding.md` for the Perl + AnyEvent binding.