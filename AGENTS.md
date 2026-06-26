# AGENTS.md — libhttp2

**Project identity**: Pure C state-machine HTTP/2 library. System-call free, callback free. All I/O, networking, and event handling lives exclusively in the calling application (typically an io_uring or epoll event loop). The library only consumes/produces byte buffers and explicit state transitions.

**Key commands** (run from repo root):
- `cmake -B build -S . && cmake --build build` — configure and build the static library + tests
- `ctest --test-dir build` — run verification tests
- `cmake --build build --target install` — install (optional)

**Documentation map** (progressive disclosure):
- AGENTS.md (this file) — start here for every task
- ARCHITECTURE.md — module boundaries, invariants, deliberate absences
- docs/README.md — full documentation index
- docs/DOMAIN.md — HTTP/2 domain glossary and workflows
- docs/decisions/ — Architecture Decision Records (ADRs)

**Operating rules**:
- Never introduce system calls, callbacks, or hidden I/O inside the library.
- State machine design follows libassh patterns: explicit states, deterministic transitions driven only by input buffers and caller-supplied context.
- Every change must keep the library buildable with `-Wall -Wextra -Wpedantic -Werror` (or MSVC equivalent) and pass existing tests.
- Prefer small, reviewable patches. Update relevant docs/ADRs when architecture or domain assumptions change.
- Hermes agent (or any coding agent) must consult AGENTS.md before editing code or docs.

**Definition of done** (for any ticket):
- Code compiles cleanly under strict warnings.
- Tests pass (`ctest`).
- AGENTS.md, ARCHITECTURE.md, and relevant docs remain accurate.
- No new syscalls or callbacks introduced.
- State machine remains pure (inputs → state/output only).

**Current status**: Fresh initialization. Core state machine skeleton + CMake + agent-ready documentation scaffold in place. Ready for Hermes-driven iterative development.

**Testing, Fuzzing & Valgrind Policy** (see ADR 003)
- Every change to `src/http2.c` or `include/http2.h` must add or update tests in `tests/`.
- Run `ctest` before considering any change complete.
- Parser / HPACK changes require a libFuzzer run (`http2_fuzz`) of several million iterations with no crashes.
- All tests must pass under Valgrind with no leaks or memory errors.

**Current Interface Direction (as of ADR 009)**
- All protocol modules should expose a consistent shape:
  - `*_config_t` (with `event_queue_size`)
  - `*_create(role)` and `*_create_with_config(role, config)`
  - `*_feed_input(ctx, data, len)`
  - `*_next_event(ctx, &event)` returning `protocol_event_t`
  - `*_get_output(ctx, buf, max)`
- `http2_next_event` is implemented and emits `HTTP2_EVENT_DATA` / `HTTP2_EVENT_HEADERS`.
- Legacy getters (`http2_current_state`, `http2_get_data`) remain for transition.

**Known Limitations / Areas for Improvement**
- HPACK: Only partial support; Huffman decoding is not yet implemented (see TODO in src/http2.c).
- HTTP/2: Limited frame coverage (no full flow control, priority, or PUSH_PROMISE yet).
- Event emission: `http2_next_event` is basic; more frame types should produce events.
- Tests: Most tests still use the legacy getter API.

When making changes, prefer extending the event-driven path over the legacy getters.

**ADR 010 Alignment (C Interfaces and Implementations + Language Bindings)**
- All public interfaces follow opaque type principles from Hanson's *C Interfaces and Implementations*.
- Public headers are designed to be FFI-friendly (simple types, no complex macros or bitfields).
- Consistent naming and clear ownership semantics are maintained across `http1`, `http2`, and `websocket`.
- When adding or modifying public functions, prefer designs that are easy to consume from other languages.
