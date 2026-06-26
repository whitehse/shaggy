# Perl Binding for libhttp2

This directory contains the Perl binding for libhttp2, optimized for use with [AnyEvent](https://metacpan.org/pod/AnyEvent).

## Status

This is an initial skeleton. It demonstrates the recommended FFI-based approach (using `FFI::Platypus`) that aligns with **ADR 010**.

## Files

- `lib/AnyEvent/HTTP2.pm` — Main module (high-level AnyEvent API)
- `docs/perl-binding.md` — Detailed documentation

## Design Notes

- Uses opaque handles (`http2_ctx_t *`)
- Prefers `http2_next_event` + `protocol_event_t`
- Designed to be easily extended to other languages (Rust, Python, Go, etc.)

## Future Work

- Full `protocol_event_t` structure mapping
- Proper `http2_config_t` allocation
- TLS integration via AnyEvent::TLS
- Comprehensive test suite

See the top-level `docs/language-bindings.md` for general binding guidelines.