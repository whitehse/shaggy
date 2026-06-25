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
