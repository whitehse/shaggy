# ADR 006: Core Library as Plumbing — PDU Parsing with Unencapsulated Data Exposure

**Date**: 2026-06-24  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
Previous decisions (ADR 004 and ADR 005) established client+server symmetry and allowed HTTP/1.1 + WebSocket as sibling libraries. However, the existing HTTP/2 implementation (and the newly added HTTP/1.1/WebSocket modules) still contain a non-trivial amount of "active" behavior:

- Auto-generation of SETTINGS ACKs
- Canned response generation inside `http2_get_output`
- Automatic PONG replies in the WebSocket layer
- Implicit state transitions that produce output without explicit caller instruction

This mixes **protocol mechanics** with **application policy**, violating the goal of a clean separation.

## Decision
The core libraries (`http2`, `http1`, `websocket`, and any future protocol modules) shall act strictly as **plumbing**.

### Key Principles

1. **Minimal Active Code**
   - The library should contain as little "active" logic as possible.
   - It is an expert at **parsing and serializing Protocol Data Units (PDUs)** — frames, headers, messages, etc.
   - It does **not** decide what to do with the data.

2. **Unencapsulated Data Exposure**
   - Parsed data is returned to the caller in its **unencapsulated form** (e.g., complete headers, message payloads, stream data).
   - The library exposes higher-level transmitted content without requiring the caller to understand the underlying protocol details.

3. **Event-Driven Return of Data (libassh-inspired)**
   - Instead of the library performing actions, it **emits** structured data to the caller (via return values, output buffers, or explicit event structures).
   - The calling application is responsible for acting on this data or forwarding it to other specialized libraries.

4. **Networking Stack Model**
   - The library behaves like a protocol layer in a stack.
   - HTTP/2 or WebSocket data is reduced to raw application payloads.
   - A higher-level library (or the application itself) can consume this data without any knowledge of HTTP/2 framing, HPACK, WebSocket masking, etc.

5. **Caller Owns All Policy and Side Effects**
   - All networking I/O, file operations, system calls, and decision-making live exclusively in the calling application.
   - The library only transforms bytes ↔ structured protocol data.

## Rationale
- Maximizes reusability and testability.
- Allows the same core PDU parsers to be used by many different applications and higher-level libraries.
- Prevents the core from accumulating domain-specific behavior (e.g., "auto-respond with 200", "auto-PONG").
- Aligns with the original libassh-inspired design goal of explicit, deterministic state machines that surface events rather than act on them.

## Consequences

- Existing "active" behaviors in `http2.c`, `http1.c`, and `websocket.c` (auto-ACKs, canned responses, auto-PONGs) should be considered technical debt and gradually removed or made optional/explicit.
- New APIs should favor returning parsed data structures or events to the caller rather than internally generating responses.
- Future protocol modules must be designed with this plumbing mindset from the start.
- Documentation and examples must clearly show the separation between the protocol library and the application layer that consumes its output.

## Verification
- Code reviews will check that new functionality stays within PDU parsing/serialization boundaries.
- The amount of "active" decision-making code in the core libraries should trend downward over time.
- Dialectic tests should continue to demonstrate that two protocol contexts can exchange data purely through buffer handoff, with all higher-level logic residing in the test harness (acting as the "application").

This decision elevates the "pure state machine" principle into a full **networking stack philosophy**.