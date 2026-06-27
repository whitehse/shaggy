# ADR 011: Nefarious Man-in-the-Middle Adversarial Testing Harness

**Date**: 2026-06-27  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
As the HTTP/2 library matures and gains more protocol coverage (including extended frame types, HPACK compression/decompression, flow control, priority handling, and SETTINGS/GOAWAY/PING/PUSH_PROMISE interactions), it becomes increasingly important to test its robustness against malicious or malformed input.

Traditional unit and dialectic tests (see ADR 004) are valuable but tend to be cooperative. To properly stress the protocol state machine, frame parser, HPACK decoder, and connection lifecycle logic, we need an **active adversary** that can sit between a legitimate client and server and deliberately attempt to break or compromise the session.

This aligns with the existing emphasis on fuzzing and Valgrind in ADR 003, and the consistent event-driven interfaces in ADR 009.

## Decision
We will implement a **nefarious Man-in-the-Middle (MITM)** testing harness as part of the project. This component will:

- Sit between two `http2_ctx_t` instances (client and server).
- Intercept, inspect, mutate, drop, reorder, duplicate, or inject HTTP/2 frames and HPACK data.
- Provide configurable attack strategies (e.g., frame length corruption, HPACK Huffman/bit-flip attacks, invalid stream IDs, flow-control window manipulation, state machine confusion via unexpected frame ordering, PING/SETTINGS abuse).
- Be usable both in unit tests and as a foundation for fuzzing (`http2_fuzz`).
- Respect the pure "plumbing" contract (ADR 006): the core library remains syscall-free and callback-free; the MITM operates entirely in test code.

The MITM will be implemented in a way that keeps the core library (`src/http2.c`, `include/http2.h`) clean while providing powerful adversarial testing capabilities. New code lives under `tests/mitm/`.

## Rationale
- Protocol implementations (especially HTTP/2 with its complex framing and compression) are security-critical.
- Cooperative tests often miss edge cases that real attackers would exploit (e.g., HPACK desync, stream state violations, connection preface abuse).
- A dedicated adversarial harness aligns with the project's strong emphasis on robustness, edge-case discovery, and input validation.
- It provides a natural, state-aware path toward structured fuzzing that is more effective than raw byte-level fuzzing for stateful protocols.
- The approach mirrors proven patterns from sibling protocol projects and supports the dialectic (client+server) testing style already in use.

## Consequences
- New files will be added under `tests/mitm/` (e.g., `mitm.h`, `mitm.c`, attack strategy modules).
- The MITM may observe `protocol_event_t` emissions and raw frame buffers without modifying the public API.
- Fuzzing campaigns (`http2_fuzz`) will be able to use the MITM as a mutation engine for targeted adversarial inputs.
- Documentation, ADRs, and the `adversarial-protocol-testing` skill will reference this approach.
- Existing tests and the strict build flags (`-Wall -Wextra -Wpedantic -Werror`) must remain unaffected.
- Future frame-type coverage (PUSH_PROMISE, CONTINUATION, etc.) will include corresponding adversarial test cases.

## Verification
- The MITM will be exercised in dedicated test cases under `tests/`.
- Fuzzing runs using the MITM (several million iterations) should demonstrate discovery of previously untested edge cases with no crashes or memory errors under Valgrind.
- All existing `ctest` tests must continue to pass.
- The harness will support both passive observation and active mutation modes.

This decision formalizes the commitment to proactive, adversarial testing of the HTTP/2 state machine implementation, extending the project's existing robustness practices.