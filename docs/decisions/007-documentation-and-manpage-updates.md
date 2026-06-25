# ADR 007: Documentation and Manpage Updates Required with Functional or API Changes

**Date**: 2026-06-24  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
The project has grown from a single HTTP/2 core into a family of sibling protocol libraries (http2, http1, websocket) with a strong emphasis on the plumbing model, explicit `protocol_event_t` events, configurable initialization via `*_config_t` structs, and a complete set of section-3 manpages.

Previous ADRs (especially 005 and 006) and the agent-ready scaffold already stress keeping docs accurate. However, there was no explicit rule tying documentation updates to code changes. This has led to occasional drift between implementation (return codes, new events, config options, send/ event semantics) and the written record (ARCHITECTURE.md, DOMAIN.md, manpages).

## Decision
**Any functional change or public API modification must be accompanied by corresponding documentation updates in the same patch.**

This includes, but is not limited to:
- New or changed functions, parameters, or return values (including error cases and NULL handling)
- New or modified events in `protocol_events.h` (new `WS_EVENT_*` / `HTTP1_EVENT_*` types, changes to `ws_message_t`, `ws_close_t`, etc.)
- Changes to config structs (`websocket_config_t`, `http1_config_t`, …) or creation semantics (`*_create_with_config`)
- Behavioral changes (state transitions, queue semantics, fragmentation, close handling)
- New protocol features or sibling modules

**Required updates in the same change**:
1. Update or add entries in `docs/decisions/` when the architectural assumption changes.
2. Revise `ARCHITECTURE.md` (module boundaries, entry points, invariants, plumbing model).
3. Revise `DOMAIN.md` (new concepts, event descriptions, workflows).
4. Update `docs/README.md` index if new manpages or sections are added.
5. **Update the relevant manpages in `man/man3/`**:
   - Add or revise sections for affected functions (SYNOPSIS, DESCRIPTION, RETURN VALUES, EXAMPLES, NOTES, SEE ALSO).
   - Ensure return codes, const-data lifetimes, config options, and caller responsibilities are accurately documented.
   - Keep manpage dates and version strings current.
6. Optionally update root `README.md`, `AGENTS.md` (definition of done), or examples if they demonstrate the API.

The change is not considered complete until the documentation and manpages have been updated, reviewed, and the tests still pass (`ctest`).

## Rationale
- A library's primary deliverable to users is its **documented API**. Code that works but whose documentation is stale is effectively broken for consumers.
- Manpages are first-class artifacts (installed via CMake, referenced from the docs index) and must not lag behind the implementation.
- Enforces the "agent-ready" and reviewable-patch philosophy: every small change remains self-contained and reviewable.
- Prevents the accumulation of technical debt in the documentation layer (the same principle applied to the code itself in ADR 006).
- Supports downstream users, IDEs, and `man` / `apropos` tooling.

## Consequences
- **Definition of done** (AGENTS.md) is extended: "Documentation and manpages updated for any API or behavioral change."
- Code review checklist now explicitly includes manpage and doc diffs.
- Future functional work (new opcodes, config fields, event types, error paths) will always produce a documentation delta.
- Manpage maintenance becomes part of the normal development workflow rather than a separate task.
- If a change is purely internal (no public API impact), documentation updates are still encouraged but not mandatory.

## Verification
- Every PR that touches `src/*.c`, `include/*.h` (except untouched `http2.h`/`http2.c` per prior rule), or adds new public symbols must also modify at least one `.md` file and the relevant `man/man3/*.3` file(s).
- `ctest` + documentation build/lint (if added later) must pass.
- The new ADR itself is self-referential: it was created together with the requirement to keep manpages current.

This decision elevates documentation and manpages to the same engineering rigor as the protocol state machines themselves.