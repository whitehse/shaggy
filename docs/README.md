# docs/README.md — libhttp2 Documentation Index

Progressive disclosure documentation:

- [AGENTS.md](../AGENTS.md) (root) — Start here: build, test, agent rules, definition of done.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Module boundaries, invariants, plumbing philosophy (ADR 006), event model, siblings (http1, websocket per ADR 005).
- [DOMAIN.md](DOMAIN.md) — Protocol domain glossary, events (`protocol_event_t`), workflows for HTTP/2 + HTTP/1.1 + WebSocket.
- [decisions/](decisions/) — Architecture Decision Records
  - 001-agent-ready-documentation.md
  - 002-event-loop-compatibility.md
  - 003-testing-fuzzing-valgrind.md (ctest + Valgrind + fuzz mandatory for core changes)
  - 004-dialectic-client-server-testing.md
  - 005-http1-websocket-sibling-libraries.md
  - 006-core-library-as-plumbing-pdu-stack.md
  - 007-documentation-and-manpage-updates.md — Documentation and manpages must be updated together with any functional or API change

## API Documentation
- Manpages (section 3): See `man/man3/` (or installed under ${CMAKE_INSTALL_MANDIR}/man3). These document C calling conventions, return codes (NULL, 0/-1, byte counts, event 0/1), event data lifetimes, configurable initialization via `*_config_t`, and the plumbing/event model.
  - libhttp2.3 — Overview and common patterns across modules.
  - websocket_create.3 — Lifecycle, `create`/`create_with_config` (event_queue_size), destroy, reset.
  - websocket_next_event.3 — Event dequeuing (`protocol_event_t`), return semantics, unencapsulated data.
  - websocket_feed_input.3, websocket_get_output.3 — Buffer I/O surfaces and return values.
  - websocket_send.3 — All send helpers (text/binary/ping/pong/close/frame) with error returns.
  - websocket_destroy.3 — Cleanup and state queries.
  - (http1 equivalents follow identical conventions; see headers and source for signatures.)
- Headers in `include/`: websocket.h, http1.h, protocol_events.h, http2.h (core untouched).
- All public functions follow strict C conventions: check returns for NULL/0/-1/bytes, events carry const data pointers (no ownership transfer unless documented).

## Harness / Testing Infrastructure
- Dialectic tests in `tests/`: paired client/server ctx exchanging buffers (no sockets).
- Examples in `examples/`: event-loop harnesses (liburing, epoll, libev, libuv) demonstrating integration without modifying library.
- CMake + ctest: builds static libs (http2, http1, websocket), runs all tests under strict -Wall -Werror.
- Valgrind + fuzz policy enforced on parser changes.

All docs and manpages emphasize the plumbing model: libraries emit structured events and raw PDU data; caller owns decisions and I/O.
## Recent Architecture Decisions

- [008-c-only-examples-and-codebase.md](decisions/008-c-only-examples-and-codebase.md) — Strict C language requirement (no C++).
- [009-consistent-protocol-interfaces.md](decisions/009-consistent-protocol-interfaces.md) — Unified `*_config_t` + `*_next_event` interface across http1/http2/websocket.
- [010-c-interfaces-and-language-bindings.md](decisions/010-c-interfaces-and-language-bindings.md) — Adopt Hanson's *C Interfaces and Implementations* principles + design for language binding friendliness.

## Language Bindings

- [language-bindings.md](language-bindings.md) — General guidance for creating FFI-friendly bindings.
- [perl-binding.md](perl-binding.md) — Perl + AnyEvent binding documentation and status.
