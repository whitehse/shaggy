# ADR 009: Consistent Event-Driven Interface for Protocol Modules

**Date**: 2026-06-26  
**Status**: Accepted  
**Deciders**: Project maintainers

## Context
`http1` and `websocket` modules already use a modern interface:
- Config struct (`http1_config_t`, `websocket_config_t`)
- `*_create_with_config(role, config)`
- `*_next_event(ctx, &event)` returning `protocol_event_t`

The original `http2` module still uses an older getter style (`http2_current_state`, `http2_get_data`, `http2_error_code`).

The user has clarified that while module separation is required, the three protocol implementations should present a **similar style of interface**.

## Decision
All protocol modules (`http2`, `http1`, `websocket`) shall converge on the following interface pattern:

```c
typedef struct {
    int event_queue_size;   /* 0 = use sensible default */
} protocol_config_t;

ctx = protocol_create_with_config(role, &config);
protocol_feed_input(ctx, data, len);
int protocol_next_event(ctx, &protocol_event_t event);
size_t protocol_get_output(ctx, buf, max);
void protocol_destroy(ctx);
```

`http2.h` will be updated to match this style (adding `http2_config_t`, `http2_next_event`, etc.) while keeping backward compatibility where reasonable during transition.

## Rationale
- Reduces cognitive load when working across protocols.
- Enables shared test harnesses and example code.
- Aligns with the "explicit event structures" preference stated in project memory.
- Still allows each module to have protocol-specific events inside the `protocol_event_t` union.

## Consequences
- `http2.h` and `src/http2.c` will be modified to implement the new interface.
- Examples and tests will be updated to use the new `next_event` style where appropriate.
- Old getter functions may be kept temporarily for compatibility or deprecated.

## Verification
- All three modules expose `*_config_t`, `*_create_with_config`, and `*_next_event`.
- Existing tests continue to pass.
- New code uses the event-driven style.