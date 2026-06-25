# ADR 008: Strict C Language Requirement for Examples and Codebase

**Date**: 2026-06-25  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
The core library (`src/http2.c`, `include/http2.h`, etc.) is implemented in pure C11 with no C++ features. The four reference examples in `examples/` demonstrate integration with different event loops while exercising the library's `feed_input` / `get_output` boundary.

During recent enhancements (multi-connection support, CLI options, OpenSSL + Linux kTLS integration), one intermediate version of an example inadvertently introduced a C++ lambda expression. This violated the project's language purity goals.

## Decision
**All code in this repository, including the example programs, must be strictly valid C (C11). No C++ features are permitted.**

This applies to:
- Core library sources and headers
- All files under `examples/`
- Test files
- Any supporting scripts or build glue that is compiled as C

Allowed: C11 features (`_Static_assert`, designated initializers, `thread_local`, etc.)
Forbidden: C++ lambdas, `auto`, range-based for, `std::` anything, `new`/`delete`, templates, etc.

## Rationale
- Maintains consistency with the "pure C state-machine" identity of the project.
- Simplifies cross-compilation, embedded use, and static analysis.
- Avoids hidden dependencies on a C++ runtime or ABI.
- Makes the examples easier for C programmers and agents to maintain and review.
- Prevents accidental introduction of C++-only constructs that would break the strict `-Werror -pedantic` build.

## Consequences
- Example code using event loops (epoll, liburing, libev, libuv) must use only C idioms (function pointers, manual loops, `struct` callbacks).
- OpenSSL + kTLS integration must be written in C (no C++ wrapper classes).
- Any future feature (e.g. WebSocket, HTTP/1.1 sibling modules) must also obey this rule.
- CI / build checks will continue to enforce `-Wall -Wextra -Wpedantic -Werror`.
- When reviewing or generating code, any use of C++ syntax must be rejected.

## Verification
- All examples compile successfully under the existing CMake strict flags after the C-only cleanup.
- New examples or significant changes must be reviewed for C purity.
- This ADR is referenced in `AGENTS.md` and future contribution guidelines.

This decision reinforces the project's commitment to being a thin, portable, pure-C protocol plumbing library.